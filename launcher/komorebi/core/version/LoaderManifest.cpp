// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/version/LoaderManifest.h"

#include <QJsonDocument>
#include <QStringList>

#include "komorebi/core/version/VersionJson.h"

namespace Komorebi::Core {

namespace {

/// The full address of a maven library: the base the profile gives plus the path its coordinate names.
QString libraryUrl(const QString& mavenBase, const QString& coordinate)
{
    QString base = mavenBase;
    if (base.isEmpty())
        base = QStringLiteral("https://maven.fabricmc.net/");
    if (!base.endsWith(QLatin1Char('/')))
        base += QLatin1Char('/');

    const QString path = libraryRelativePath(coordinate);
    return path.isEmpty() ? QString() : base + path;
}

}  // namespace

std::optional<QList<LoaderRelease>> parseFabricLoaderList(const QJsonArray& document, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<QList<LoaderRelease>> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    QList<LoaderRelease> releases;
    for (const auto& value : document) {
        const auto loader = value.toObject().value(QStringLiteral("loader")).toObject();
        LoaderRelease release;
        release.version = loader.value(QStringLiteral("version")).toString();
        release.stable = loader.value(QStringLiteral("stable")).toBool();
        if (!release.version.isEmpty())
            releases.append(release);
    }
    if (releases.isEmpty())
        return fail(QStringLiteral("Fabric lists no loader versions for this game version"));
    return releases;
}

std::optional<QJsonObject> componentFromFabricProfile(const QJsonObject& profile, const QString& loaderVersion,
                                                      QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<QJsonObject> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    if (loaderVersion.isEmpty())
        return fail(QStringLiteral("no Fabric loader version was given"));
    const QString mainClass = profile.value(QStringLiteral("mainClass")).toString();
    if (mainClass.isEmpty())
        return fail(QStringLiteral("the Fabric profile has no main class"));

    QJsonArray libraries;
    for (const auto& value : profile.value(QStringLiteral("libraries")).toArray()) {
        const auto library = value.toObject();
        const QString name = library.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
            continue;

        const QString url = libraryUrl(library.value(QStringLiteral("url")).toString(), name);
        QJsonObject artifact{ { QStringLiteral("sha1"), library.value(QStringLiteral("sha1")).toString() },
                              { QStringLiteral("size"), library.value(QStringLiteral("size")) } };
        if (!url.isEmpty())
            artifact.insert(QStringLiteral("url"), url);
        libraries.append(QJsonObject{ { QStringLiteral("name"), name },
                                      { QStringLiteral("downloads"), QJsonObject{ { QStringLiteral("artifact"), artifact } } } });
    }
    if (libraries.isEmpty())
        return fail(QStringLiteral("the Fabric profile lists no libraries"));

    QJsonObject component{
        { QStringLiteral("formatVersion"), 1 },
        { QStringLiteral("uid"), QStringLiteral("net.fabricmc.fabric-loader") },
        { QStringLiteral("version"), loaderVersion },
        { QStringLiteral("name"), QStringLiteral("Fabric Loader") },
        { QStringLiteral("type"), QStringLiteral("release") },
        { QStringLiteral("order"), 0 },
        { QStringLiteral("mainClass"), mainClass },
        { QStringLiteral("libraries"), libraries },
    };

    const auto arguments = profile.value(QStringLiteral("arguments")).toObject();
    QJsonArray jvmArguments;
    for (const auto& value : arguments.value(QStringLiteral("jvm")).toArray()) {
        if (value.isString())
            jvmArguments.append(value.toString());
    }
    component.insert(QStringLiteral("+jvmArgs"), jvmArguments);

    return component;
}

}  // namespace Komorebi::Core
