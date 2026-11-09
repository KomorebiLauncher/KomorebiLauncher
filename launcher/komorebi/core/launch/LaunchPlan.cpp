// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/launch/LaunchPlan.h"
#include "komorebi/core/version/LibraryFile.h"

#include <QDir>
#include <QHash>

namespace Komorebi::Core {

namespace {

/// The placeholders the format defines. Anything not listed here is left as it is, which is how a
/// missing value shows up in a log instead of disappearing.
QHash<QString, QString> placeholders(const LaunchOptions& options, const QString& assetIndexName, const QString& classpath)
{
    QHash<QString, QString> values;
    values.insert(QStringLiteral("auth_player_name"), options.username);
    values.insert(QStringLiteral("version_name"), options.versionName);
    values.insert(QStringLiteral("game_directory"), options.gameDirectory);
    values.insert(QStringLiteral("assets_root"), options.assetsDirectory);
    values.insert(QStringLiteral("assets_index_name"), assetIndexName);
    values.insert(QStringLiteral("auth_uuid"), options.uuid);
    values.insert(QStringLiteral("auth_access_token"), options.accessToken);
    values.insert(QStringLiteral("auth_session"), options.accessToken);
    values.insert(QStringLiteral("user_type"), options.userType);
    values.insert(QStringLiteral("version_type"), QStringLiteral("release"));
    values.insert(QStringLiteral("clientid"), options.clientId);
    values.insert(QStringLiteral("auth_xuid"), options.xuid);
    values.insert(QStringLiteral("natives_directory"), options.nativesDirectory);
    values.insert(QStringLiteral("classpath"), classpath);
    values.insert(QStringLiteral("classpath_separator"), QStringLiteral(";"));
    values.insert(QStringLiteral("library_directory"), options.librariesDirectory);
    values.insert(QStringLiteral("launcher_name"), QStringLiteral("Komorebi"));
    values.insert(QStringLiteral("launcher_version"), QStringLiteral("0.1"));
    return values;
}

QString substitute(const QString& text, const QHash<QString, QString>& values)
{
    QString result = text;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
        result.replace(QStringLiteral("${%1}").arg(it.key()), it.value());
    return result;
}

QStringList substituteAll(const QStringList& arguments, const QHash<QString, QString>& values)
{
    QStringList result;
    result.reserve(arguments.size());
    for (const auto& argument : arguments)
        result.append(substitute(argument, values));
    return result;
}

}  // namespace

QString clientJarPath(const QString& versionDirectory, const QString& versionId)
{
    return QDir(versionDirectory).filePath(versionId + QStringLiteral(".jar"));
}

QStringList classpathEntries(const VersionJson& merged, const LaunchOptions& options, const Platform& platform)
{
    QStringList entries;
    for (const auto& library : merged.libraries) {
        if (!library.appliesTo(platform))
            continue;
        const QString path = libraryFilePath(library, options.librariesDirectory);
        if (path.isEmpty())
            continue;
        entries.append(path);
    }
    if (!options.clientJarPath.isEmpty())
        entries.append(options.clientJarPath);
    return entries;
}

LaunchPlan buildLaunchPlan(const VersionJson& merged, const LaunchOptions& options, const Platform& platform)
{
    LaunchPlan plan;
    plan.workingDirectory = options.gameDirectory;

    const auto classpath = classpathEntries(merged, options, platform).join(QLatin1Char(';'));
    const QString assetIndexName = options.assetIndexName.isEmpty()
                                       ? (merged.assetIndexId.isEmpty() ? merged.assets : merged.assetIndexId)
                                       : options.assetIndexName;
    const auto values = placeholders(options, assetIndexName, classpath);

    if (options.javaPath.isEmpty())
        plan.problems.append(QStringLiteral("no Java runtime was chosen"));
    else
        plan.program = options.javaPath;

    if (merged.mainClass.isEmpty())
        plan.problems.append(QStringLiteral("the version does not say which class starts the game"));
    if (merged.libraries.isEmpty())
        plan.problems.append(QStringLiteral("the version lists no libraries"));
    if (options.clientJarPath.isEmpty())
        plan.problems.append(QStringLiteral("the version's own jar was not given"));
    if (options.nativesDirectory.isEmpty())
        plan.problems.append(QStringLiteral("no directory for the native libraries was given"));

    // Memory first, so an extra argument can still override it.
    plan.arguments << QStringLiteral("-Xms%1m").arg(options.minMemoryMb)
                   << QStringLiteral("-Xmx%1m").arg(options.maxMemoryMb);

    /*
     * Whether the manifest already tells Java where the class path is decides the shape of the
     * arguments: a Mojang-style `arguments.jvm` names it with a placeholder, while component metadata
     * only lists extra JVM flags and expects the launcher to supply the class path and the natives.
     */
    const auto jvmText = merged.jvmArguments.join(QLatin1Char(' '));
    const bool manifestNamesClasspath = jvmText.contains(QLatin1String("${classpath}")) || merged.jvmArguments.contains(QStringLiteral("-cp"));

    if (manifestNamesClasspath) {
        plan.arguments << substituteAll(merged.jvmArguments, values);
    } else {
        plan.arguments << QStringLiteral("-Djava.library.path=%1").arg(options.nativesDirectory) << QStringLiteral("-cp")
                       << classpath << QStringLiteral("-Dminecraft.launcher.brand=%1").arg(values.value(QStringLiteral("launcher_name")))
                       << QStringLiteral("-Dminecraft.launcher.version=%1").arg(values.value(QStringLiteral("launcher_version")))
                       << substituteAll(merged.jvmArguments, values);
    }

    plan.arguments << options.extraJvmArguments;
    plan.arguments << merged.mainClass;

    // The old format is one line of arguments; the new one is an array. Either way the placeholders
    // inside them are the same.
    if (!merged.legacyArguments.isEmpty())
        plan.arguments << substituteAll(merged.legacyArguments, values);
    else
        plan.arguments << substituteAll(merged.gameArguments, values);

    plan.arguments << options.extraGameArguments;

    // Joining a server or a world is a launcher feature named by the manifest's traits: newer versions
    // take it as quick play, older ones as --server/--port.
    if (!options.serverAddress.isEmpty()) {
        const auto quickPlay = merged.traits.contains(QStringLiteral("feature:is_quick_play_multiplayer"));
        if (quickPlay) {
            plan.arguments << QStringLiteral("--quickPlayMultiplayer")
                           << QStringLiteral("%1:%2").arg(options.serverAddress).arg(options.serverPort);
        } else {
            plan.arguments << QStringLiteral("--server") << options.serverAddress << QStringLiteral("--port")
                           << QString::number(options.serverPort);
        }
    } else if (!options.worldToJoin.isEmpty()
               && merged.traits.contains(QStringLiteral("feature:is_quick_play_singleplayer"))) {
        plan.arguments << QStringLiteral("--quickPlaySingleplayer") << options.worldToJoin;
    }
    return plan;
}

}  // namespace Komorebi::Core
