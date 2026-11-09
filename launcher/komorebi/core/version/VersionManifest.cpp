// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/version/VersionManifest.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>

namespace Komorebi::Core {

namespace {

/// Whether a game version is new enough to understand the quick play arguments.
bool supportsQuickPlay(const QString& id)
{
    const auto parts = id.split(QLatin1Char('.'));
    if (parts.size() < 2)
        return false;
    bool majorOk = false;
    bool minorOk = false;
    const int major = parts.at(0).toInt(&majorOk);
    const int minor = parts.at(1).section(QLatin1Char('-'), 0, 0).toInt(&minorOk);
    if (!majorOk || !minorOk)
        return false;
    if (major != 1)
        return major > 1;
    return minor >= 20;
}

QString releaseTimeFor(const QJsonObject& version)
{
    const QDateTime parsed = QDateTime::fromString(version.value(QStringLiteral("releaseTime")).toString(), Qt::ISODate);
    if (!parsed.isValid())
        return {};
    // What the launcher's component metadata shows; its own format, not ISO.
    return parsed.toString(QStringLiteral("dd/MM/yyyy HH:mm:ss"));
}

/// The game's arguments as the one legacy line the component format keeps.
QString gameArgumentsFor(const QJsonObject& version)
{
    const auto arguments = version.value(QStringLiteral("arguments")).toObject();
    const auto game = arguments.value(QStringLiteral("game")).toArray();
    QStringList parts;
    for (const auto& entry : game) {
        // Rule-carrying entries are launcher features (demo mode, custom resolution); a created instance
        // starts vanilla, so they are left out rather than applied unconditionally.
        if (entry.isString())
            parts.append(entry.toString());
    }
    if (!parts.isEmpty())
        return parts.join(QLatin1Char(' '));
    return version.value(QStringLiteral("minecraftArguments")).toString();
}

}  // namespace

std::optional<VersionManifest> parseVersionManifest(const QJsonObject& document, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<VersionManifest> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    const auto versionsValue = document.value(QStringLiteral("versions"));
    if (!versionsValue.isArray())
        return fail(QStringLiteral("the version manifest lists no versions"));

    VersionManifest manifest;
    manifest.latestRelease = document.value(QStringLiteral("latest")).toObject().value(QStringLiteral("release")).toString();
    manifest.latestSnapshot = document.value(QStringLiteral("latest")).toObject().value(QStringLiteral("snapshot")).toString();

    for (const auto& value : versionsValue.toArray()) {
        const auto object = value.toObject();
        RemoteVersion version;
        version.id = object.value(QStringLiteral("id")).toString();
        version.type = object.value(QStringLiteral("type")).toString();
        version.url = object.value(QStringLiteral("url")).toString();
        version.sha1 = object.value(QStringLiteral("sha1")).toString();
        version.releaseTime = object.value(QStringLiteral("releaseTime")).toString();
        if (!version.id.isEmpty() && !version.url.isEmpty())
            manifest.versions.append(version);
    }

    if (manifest.versions.isEmpty())
        return fail(QStringLiteral("the version manifest lists no usable versions"));
    return manifest;
}

std::optional<QJsonObject> componentFromMojangVersion(const QJsonObject& version, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<QJsonObject> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    const QString id = version.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return fail(QStringLiteral("the version document has no id"));
    const QString mainClass = version.value(QStringLiteral("mainClass")).toString();
    if (mainClass.isEmpty())
        return fail(QStringLiteral("the version document has no main class"));
    const auto client = version.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("client")).toObject();
    if (client.isEmpty())
        return fail(QStringLiteral("the version document has no client jar"));

    QJsonObject component{
        { QStringLiteral("formatVersion"), 1 },
        { QStringLiteral("uid"), QStringLiteral("net.minecraft") },
        { QStringLiteral("version"), id },
        { QStringLiteral("name"), QStringLiteral("Minecraft") },
        { QStringLiteral("type"), version.value(QStringLiteral("type")).toString() },
        { QStringLiteral("order"), -2 },
        { QStringLiteral("mainClass"), mainClass },
        { QStringLiteral("mainJar"),
          QJsonObject{ { QStringLiteral("name"), QStringLiteral("com.mojang:minecraft:%1:client").arg(id) },
                       { QStringLiteral("downloads"), QJsonObject{ { QStringLiteral("artifact"), client } } } } },
        { QStringLiteral("libraries"), version.value(QStringLiteral("libraries")) },
    };

    const QString releaseTime = releaseTimeFor(version);
    if (!releaseTime.isEmpty())
        component.insert(QStringLiteral("releaseTime"), releaseTime);

    if (const auto assetIndex = version.value(QStringLiteral("assetIndex")).toObject(); !assetIndex.isEmpty())
        component.insert(QStringLiteral("assetIndex"), assetIndex);

    const auto javaVersion = version.value(QStringLiteral("javaVersion")).toObject();
    const int javaMajor = javaVersion.value(QStringLiteral("majorVersion")).toInt();
    if (javaMajor > 0)
        component.insert(QStringLiteral("compatibleJavaMajors"), QJsonArray{ javaMajor });
    if (const auto javaName = javaVersion.value(QStringLiteral("component")).toString(); !javaName.isEmpty())
        component.insert(QStringLiteral("compatibleJavaName"), javaName);

    if (const auto minecraftArguments = gameArgumentsFor(version); !minecraftArguments.isEmpty())
        component.insert(QStringLiteral("minecraftArguments"), minecraftArguments);

    if (const auto logging = version.value(QStringLiteral("logging")).toObject(); !logging.isEmpty())
        component.insert(QStringLiteral("logging"), logging);

    // The component format always carries a JVM-argument list, empty when the version adds none; the
    // launcher's component reader expects the field to be there.
    component.insert(QStringLiteral("+jvmArgs"), QJsonArray{});

    // No requirement is declared for the graphics library. The launcher's own components ask for it as a
    // separate component, but this one carries the libraries itself; declaring the requirement without
    // that component would make the launcher try to install one before the instance could be opened.

    if (supportsQuickPlay(id)) {
        component.insert(QStringLiteral("+traits"),
                         QJsonArray{ QStringLiteral("feature:is_quick_play_multiplayer"),
                                     QStringLiteral("feature:is_quick_play_singleplayer") });
    }

    return component;
}

}  // namespace Komorebi::Core
