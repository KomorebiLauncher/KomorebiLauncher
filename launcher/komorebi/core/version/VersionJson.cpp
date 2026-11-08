// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/version/VersionJson.h"

#include <QJsonValue>
#include <QSysInfo>

namespace Komorebi::Core {

namespace {

/// Reads "arguments.game"/"arguments.jvm": plain strings are kept, conditional objects are not yet
/// part of this slice (they only matter for arguments that a platform decides, such as a macOS flag).
QStringList readArgumentArray(const QJsonValue& value)
{
    QStringList arguments;
    for (const auto& entry : value.toArray()) {
        if (entry.isString())
            arguments.append(entry.toString());
    }
    return arguments;
}

QList<Rule> readRules(const QJsonValue& value)
{
    QList<Rule> rules;
    for (const auto& entry : value.toArray()) {
        const auto object = entry.toObject();
        Rule rule;
        rule.action = object.value(QStringLiteral("action")).toString();
        const auto os = object.value(QStringLiteral("os")).toObject();
        rule.osName = os.value(QStringLiteral("name")).toString();
        rule.osArch = os.value(QStringLiteral("arch")).toString();
        if (!rule.action.isEmpty())
            rules.append(rule);
    }
    return rules;
}

LibraryEntry readLibrary(const QJsonObject& object)
{
    LibraryEntry library;
    library.name = object.value(QStringLiteral("name")).toString();

    const auto artifact = object.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("artifact")).toObject();
    library.path = artifact.value(QStringLiteral("path")).toString();
    library.url = artifact.value(QStringLiteral("url")).toString();
    library.sha1 = artifact.value(QStringLiteral("sha1")).toString();
    library.size = static_cast<qint64>(artifact.value(QStringLiteral("size")).toDouble());

    library.rules = readRules(object.value(QStringLiteral("rules")));
    library.natives = object.value(QStringLiteral("natives")).toObject();
    for (const auto& excluded : object.value(QStringLiteral("extract")).toObject().value(QStringLiteral("exclude")).toArray()) {
        if (excluded.isString())
            library.extractExclude.append(excluded.toString());
    }
    return library;
}

QStringList splitLegacyArguments(const QString& line)
{
    QStringList arguments;
    // minecraftArguments is one string with spaces; a quoted value survives as one argument.
    QString current;
    bool quoted = false;
    for (const QChar character : line) {
        if (character == QLatin1Char('"')) {
            quoted = !quoted;
            continue;
        }
        if (character.isSpace() && !quoted) {
            if (!current.isEmpty())
                arguments.append(current);
            current.clear();
            continue;
        }
        current.append(character);
    }
    if (!current.isEmpty())
        arguments.append(current);
    return arguments;
}

}  // namespace

Platform Platform::current()
{
    Platform platform;
#if defined(Q_OS_WIN)
    platform.os = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    platform.os = QStringLiteral("osx");
#else
    platform.os = QStringLiteral("linux");
#endif
    const auto arch = QSysInfo::currentCpuArchitecture();
    if (arch == QLatin1String("arm64"))
        platform.arch = QStringLiteral("arm64");
    else if (arch == QLatin1String("x86_64"))
        platform.arch = QStringLiteral("x86_64");
    else
        platform.arch = arch;
    return platform;
}

bool LibraryEntry::appliesTo(const Platform& platform) const
{
    // No rules: the format allows the entry everywhere.
    if (rules.isEmpty())
        return true;

    bool allowed = false;
    for (const auto& rule : rules) {
        const bool matchesName = rule.osName.isEmpty() || rule.osName == platform.os;
        const bool matchesArch = rule.osArch.isEmpty() || rule.osArch == platform.arch;
        if (matchesName && matchesArch)
            allowed = rule.action == QLatin1String("allow");
    }
    return allowed;
}

QString LibraryEntry::nativesClassifier(const Platform& platform) const
{
    QString classifier = natives.value(platform.os).toString();
    if (classifier.isEmpty())
        return {};
    // The classifier may still name the architecture, e.g. "natives-windows-${arch}".
    return classifier.replace(QLatin1String("${arch}"), platform.arch == QLatin1String("x86_64") ? QStringLiteral("64")
                                                                                                  : platform.arch);
}

std::optional<VersionJson> VersionJson::parse(const QJsonObject& object, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<VersionJson> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    if (object.isEmpty())
        return fail(QStringLiteral("the manifest is empty"));

    VersionJson manifest;
    manifest.id = object.value(QStringLiteral("id")).toString();
    if (manifest.id.isEmpty()) {
        // Component metadata (what this launcher keeps for every version) names the version as a
        // uid/version pair instead of an id; the game version is what a manifest is known by.
        const auto uid = object.value(QStringLiteral("uid")).toString();
        const auto version = object.value(QStringLiteral("version")).toString();
        if (uid.isEmpty() || version.isEmpty())
            return fail(QStringLiteral("the manifest has neither an id nor a uid/version pair"));
        manifest.id = uid == QLatin1String("net.minecraft") ? version : uid + QLatin1Char('-') + version;
    }

    manifest.inheritsFrom = object.value(QStringLiteral("inheritsFrom")).toString();
    manifest.mainClass = object.value(QStringLiteral("mainClass")).toString();
    manifest.type = object.value(QStringLiteral("type")).toString();
    manifest.assets = object.value(QStringLiteral("assets")).toString();

    const auto assetIndexValue = object.value(QStringLiteral("assetIndex"));
    if (assetIndexValue.isObject()) {
        manifest.assetIndex = assetIndexValue.toObject();
        manifest.assetIndexId = manifest.assetIndex.value(QStringLiteral("id")).toString();
    }
    manifest.downloads = object.value(QStringLiteral("downloads")).toObject();
    manifest.mainJarName = object.value(QStringLiteral("mainJar")).toObject().value(QStringLiteral("name")).toString();

    const auto javaVersion = object.value(QStringLiteral("javaVersion")).toObject();
    manifest.javaMajor = javaVersion.value(QStringLiteral("majorVersion")).toInt();
    manifest.javaComponent = javaVersion.value(QStringLiteral("component")).toString();
    if (manifest.javaMajor == 0) {
        // Component metadata carries the majors instead of one javaVersion object, and a version that
        // only works on one major lists it as a number rather than an array.
        const auto majors = object.value(QStringLiteral("compatibleJavaMajors"));
        if (majors.isArray()) {
            if (const auto list = majors.toArray(); !list.isEmpty())
                manifest.javaMajor = list.first().toInt();
        } else if (majors.isDouble()) {
            manifest.javaMajor = majors.toInt();
        }
    }
    // Component metadata also names the JVM arguments it adds; they belong after the manifest's own.
    manifest.jvmArguments = readArgumentArray(object.value(QStringLiteral("+jvmArgs")));

    for (const auto& entry : object.value(QStringLiteral("libraries")).toArray()) {
        const auto library = readLibrary(entry.toObject());
        if (!library.name.isEmpty())
            manifest.libraries.append(library);
    }

    const auto arguments = object.value(QStringLiteral("arguments")).toObject();
    manifest.gameArguments = readArgumentArray(arguments.value(QStringLiteral("game")));
    manifest.jvmArguments += readArgumentArray(arguments.value(QStringLiteral("jvm")));
    manifest.legacyArguments = splitLegacyArguments(object.value(QStringLiteral("minecraftArguments")).toString());

    return manifest;
}

std::optional<VersionJson> mergeChain(const QList<VersionJson>& childToParent, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<VersionJson> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    if (childToParent.isEmpty())
        return fail(QStringLiteral("there is no manifest to merge"));

    if (childToParent.size() == 1 && !childToParent.first().inheritsFrom.isEmpty())
        return fail(QStringLiteral("%1 inherits from %2, but %2 was not given")
                        .arg(childToParent.first().id, childToParent.first().inheritsFrom));

    VersionJson merged = childToParent.first();
    merged.inheritsFrom.clear();

    QStringList childFirstLibraries;
    QList<LibraryEntry> libraries;

    // Arguments accumulate from the base upwards: what the game version says, then what a loader adds.
    QStringList gameArguments;
    for (int index = childToParent.size() - 1; index >= 0; --index)
        gameArguments.append(childToParent.at(index).gameArguments);
    QStringList jvmArguments;
    for (int index = childToParent.size() - 1; index >= 0; --index)
        jvmArguments.append(childToParent.at(index).jvmArguments);

    // Libraries are added child first and de-duplicated by name: the child's copy of a library is the
    // one it was built against.
    for (const auto& manifest : childToParent) {
        for (const auto& library : manifest.libraries) {
            const QString key = library.name + QLatin1Char('|') + library.natives.value(QStringLiteral("windows")).toString();
            if (childFirstLibraries.contains(key))
                continue;
            childFirstLibraries.append(key);
            libraries.append(library);
        }
    }

    for (int index = 1; index < childToParent.size(); ++index) {
        const auto& parent = childToParent.at(index);
        if (merged.mainClass.isEmpty())
            merged.mainClass = parent.mainClass;
        if (merged.assets.isEmpty())
            merged.assets = parent.assets;
        if (merged.assetIndexId.isEmpty()) {
            merged.assetIndexId = parent.assetIndexId;
            merged.assetIndex = parent.assetIndex;
        }
        if (merged.downloads.isEmpty())
            merged.downloads = parent.downloads;
        if (merged.javaMajor == 0)
            merged.javaMajor = parent.javaMajor;
        if (merged.javaComponent.isEmpty())
            merged.javaComponent = parent.javaComponent;
        // A loader rewrites the whole old-style argument line, so the child's replaces the parent's.
        if (merged.legacyArguments.isEmpty())
            merged.legacyArguments = parent.legacyArguments;
    }

    merged.libraries = libraries;
    merged.gameArguments = gameArguments;
    merged.jvmArguments = jvmArguments;
    return merged;
}

QString libraryRelativePath(const QString& name)
{
    // group:artifact:version[:classifier][@extension]
    QString spec = name;
    const int at = spec.indexOf(QLatin1Char('@'));
    if (at >= 0)
        spec = spec.left(at);
    const auto parts = spec.split(QLatin1Char(':'));
    if (parts.size() < 3 || parts.size() > 4)
        return {};

    const QString& group = parts.at(0);
    const QString& artifact = parts.at(1);
    const QString& version = parts.at(2);
    const QString classifier = parts.size() == 4 ? parts.at(3) : QString();

    QString path = group;
    path.replace(QLatin1Char('.'), QLatin1Char('/'));
    path += QLatin1Char('/') + artifact + QLatin1Char('/') + version + QLatin1Char('/') + artifact + QLatin1Char('-') + version;
    if (!classifier.isEmpty())
        path += QLatin1Char('-') + classifier;
    path += QStringLiteral(".jar");
    return path;
}

QString libraryPath(const LibraryEntry& entry)
{
    if (!entry.path.isEmpty())
        return entry.path;
    return libraryRelativePath(entry.name);
}

}  // namespace Komorebi::Core
