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

std::optional<QList<LoaderRelease>> parseQuiltLoaderList(const QJsonArray& document, QString* error)
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
        release.stable = !release.version.contains(QStringLiteral("-"));
        if (!release.version.isEmpty())
            releases.append(release);
    }
    if (releases.isEmpty())
        return fail(QStringLiteral("Quilt lists no loader versions for this game version"));
    return releases;
}

std::optional<QJsonObject> componentFromLoaderProfile(const QJsonObject& profile, const QString& uid, const QString& name,
                                                      const QString& loaderVersion, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<QJsonObject> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    if (loaderVersion.isEmpty())
        return fail(QStringLiteral("no loader version was given"));
    const QString mainClass = profile.value(QStringLiteral("mainClass")).toString();
    if (mainClass.isEmpty())
        return fail(QStringLiteral("the loader profile has no main class"));

    QJsonArray libraries;
    for (const auto& value : profile.value(QStringLiteral("libraries")).toArray()) {
        const auto library = value.toObject();
        const QString coordinate = library.value(QStringLiteral("name")).toString();
        if (coordinate.isEmpty())
            continue;

        const QString url = libraryUrl(library.value(QStringLiteral("url")).toString(), coordinate);
        QJsonObject artifact{ { QStringLiteral("sha1"), library.value(QStringLiteral("sha1")).toString() },
                              { QStringLiteral("size"), library.value(QStringLiteral("size")) } };
        if (!url.isEmpty())
            artifact.insert(QStringLiteral("url"), url);
        libraries.append(QJsonObject{ { QStringLiteral("name"), coordinate },
                                      { QStringLiteral("downloads"), QJsonObject{ { QStringLiteral("artifact"), artifact } } } });
    }
    if (libraries.isEmpty())
        return fail(QStringLiteral("the loader profile lists no libraries"));

    QJsonObject component{
        { QStringLiteral("formatVersion"), 1 },
        { QStringLiteral("uid"), uid },
        { QStringLiteral("version"), loaderVersion },
        { QStringLiteral("name"), name },
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

std::optional<QJsonObject> componentFromForgeVersion(const QJsonObject& version, const QString& forgeVersion, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<QJsonObject> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    if (forgeVersion.isEmpty())
        return fail(QStringLiteral("no Forge version was given"));
    const QString mainClass = version.value(QStringLiteral("mainClass")).toString();
    if (mainClass.isEmpty())
        return fail(QStringLiteral("the Forge version has no main class"));
    const auto libraries = version.value(QStringLiteral("libraries")).toArray();
    if (libraries.isEmpty())
        return fail(QStringLiteral("the Forge version lists no libraries"));

    const auto arguments = version.value(QStringLiteral("arguments")).toObject();
    QStringList gameArguments;
    for (const auto& value : arguments.value(QStringLiteral("game")).toArray()) {
        if (value.isString())
            gameArguments.append(value.toString());
    }
    QJsonArray jvmArguments;
    for (const auto& value : arguments.value(QStringLiteral("jvm")).toArray()) {
        if (value.isString())
            jvmArguments.append(value.toString());
    }

    QJsonObject component{
        { QStringLiteral("formatVersion"), 1 },
        { QStringLiteral("uid"), QStringLiteral("net.minecraftforge") },
        { QStringLiteral("version"), forgeVersion },
        { QStringLiteral("name"), QStringLiteral("Forge") },
        { QStringLiteral("type"), QStringLiteral("release") },
        { QStringLiteral("order"), 1 },
        { QStringLiteral("mainClass"), mainClass },
        { QStringLiteral("libraries"), libraries },
        { QStringLiteral("+jvmArgs"), jvmArguments },
    };
    if (!gameArguments.isEmpty())
        component.insert(QStringLiteral("minecraftArguments"), gameArguments.join(QLatin1Char(' ')));

    return component;
}

}  // namespace Komorebi::Core
