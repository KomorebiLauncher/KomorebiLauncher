// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/install/InstanceCreator.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QTimer>
#include <QRegularExpression>

#include "FileSystem.h"
#include "komorebi/core/asset/AssetIndex.h"
#include "komorebi/core/version/LoaderManifest.h"
#include "komorebi/core/version/VersionManifest.h"
#include "komorebi/services/download/AssetFiles.h"
#include "komorebi/services/download/DownloadQueue.h"
#include "komorebi/services/download/FileDownloader.h"
#include "komorebi/services/download/InstanceFiles.h"
#include "komorebi/services/java/JavaRuntime.h"

namespace Komorebi::Services {

namespace {

/// A directory name for the instance: the player's name with what a filesystem would refuse taken out.
QString directoryNameFor(const QString& name)
{
    QString result = name;
    result.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    result = result.trimmed();
    if (result.isEmpty())
        result = QStringLiteral("Instance");
    return result;
}

QString uniqueInstanceId(const QDir& instancesRoot, const QString& name)
{
    const QString base = directoryNameFor(name);
    QString candidate = base;
    int suffix = 2;
    while (QFileInfo::exists(instancesRoot.filePath(candidate))) {
        candidate = QStringLiteral("%1-%2").arg(base).arg(suffix);
        ++suffix;
    }
    return candidate;
}

}  // namespace

InstanceCreator::InstanceCreator(QNetworkAccessManager* network, QObject* parent) : QObject(parent), m_network(network)
{
}

void InstanceCreator::start(const InstanceCreateRequest& request)
{
    if (m_running)
        return;
    m_request = request;
    m_running = true;
    m_reported = false;
    m_instanceDirectory.clear();
    m_assetIndexId.clear();
    m_loaderMetaBase.clear();
    m_loaderUid.clear();
    m_loaderName.clear();
    if (m_request.loader == QLatin1String("fabric")) {
        m_loaderMetaBase = m_request.loaderMetaUrl;
        m_loaderUid = QStringLiteral("net.fabricmc.fabric-loader");
        m_loaderName = QStringLiteral("Fabric Loader");
    } else if (m_request.loader == QLatin1String("quilt")) {
        m_loaderMetaBase = m_request.quiltMetaUrl;
        m_loaderUid = QStringLiteral("org.quiltmc.quilt-loader");
        m_loaderName = QStringLiteral("Quilt Loader");
    } else if (m_request.loader == QLatin1String("forge")) {
        m_loaderMetaBase = QString();
        m_loaderUid = QStringLiteral("net.minecraftforge");
        m_loaderName = QStringLiteral("Forge");
    } else if (m_request.loader == QLatin1String("neoforge")) {
        m_loaderMetaBase = m_request.neoforgeMetaUrl;
        m_loaderUid = QStringLiteral("net.neoforged");
        m_loaderName = QStringLiteral("NeoForge");
    } else if (!m_request.loader.isEmpty()) {
        m_running = false;
        m_reported = true;
        emit failed(tr("only the Fabric, Quilt, Forge and NeoForge loaders are supported so far, not %1")
                        .arg(m_request.loader));
        return;
    }
    fetchManifest();
}

void InstanceCreator::fail(const QString& reason)
{
    if (m_reported)
        return;
    m_running = false;
    m_reported = true;
    emit failed(reason);
}

void InstanceCreator::finish()
{
    if (m_reported)
        return;
    m_running = false;
    m_reported = true;
    emit finished(m_instanceDirectory);
}

void InstanceCreator::fetch(const QString& url, const QString& target, const QString& sha1, const std::function<void()>& next)
{
    auto* downloader = new FileDownloader(m_network, this);
    const QByteArray expected = sha1.isEmpty() ? QByteArray() : QByteArray::fromHex(sha1.toLatin1());
    QObject::connect(downloader, &FileDownloader::finished, this, [this, downloader, next](bool ok, const QString& error) {
        downloader->deleteLater();
        if (!m_running)
            return;
        if (!ok)
            return fail(error);
        next();
    });
    downloader->start(FileDownload{ sourceUrl(QUrl(url), m_request.source), target, expected, -1 });
}

void InstanceCreator::fetchManifest()
{
    emit status(tr("Reading the list of Minecraft versions"));

    const QDir data(m_request.dataRoot);
    QDir().mkpath(data.filePath(QStringLiteral("tmp")));
    m_manifestPath = data.filePath(QStringLiteral("tmp/version_manifest_v2.json"));
    m_versionsRoot = data.filePath(QStringLiteral("meta"));

    fetch(m_request.versionManifestUrl, m_manifestPath, {}, [this] {
        QFile file(m_manifestPath);
        if (!file.open(QIODevice::ReadOnly))
            return fail(tr("could not read %1").arg(m_manifestPath));

        const auto document = QJsonDocument::fromJson(file.readAll());
        QString error;
        const auto manifest = Core::parseVersionManifest(document.object(), &error);
        if (!manifest)
            return fail(error);

        for (const auto& version : manifest->versions) {
            if (version.id == m_request.minecraftVersion) {
                m_versionId = version.id;
                return fetchVersion(version.url, version.sha1);
            }
        }
        fail(tr("%1 is not in the version list").arg(m_request.minecraftVersion));
    });
}

void InstanceCreator::fetchVersion(const QString& url, const QString& sha1)
{
    emit status(tr("Reading Minecraft %1").arg(m_versionId));

    const QString target = QDir(m_request.dataRoot).filePath(QStringLiteral("tmp/version-%1.json").arg(m_versionId));
    fetch(url, target, sha1, [this, target] {
        QFile file(target);
        if (!file.open(QIODevice::ReadOnly))
            return fail(tr("could not read %1").arg(target));

        const auto document = QJsonDocument::fromJson(file.readAll());
        QString error;
        const auto component = Core::componentFromMojangVersion(document.object(), &error);
        if (!component)
            return fail(error);
        m_gameComponent = *component;
        m_gameReleaseTime = document.object().value(QStringLiteral("releaseTime")).toString();

        if (m_request.loader.isEmpty())
            return writeInstance();
        if (m_request.loader == QLatin1String("forge"))
            return installForge();
        if (m_request.loader == QLatin1String("neoforge"))
            return fetchNeoForgeIndex();
        fetchLoaderList();
    });
}

void InstanceCreator::fetchNeoForgeIndex()
{
    emit status(tr("Reading the %1 versions").arg(m_loaderName));

    const QString url = m_loaderMetaBase + QStringLiteral("/net.neoforged/index.json");
    const QString target = QDir(m_request.dataRoot).filePath(QStringLiteral("tmp/neoforge-index.json"));
    fetch(url, target, {}, [this, target] {
        QFile file(target);
        if (!file.open(QIODevice::ReadOnly))
            return fail(tr("could not read %1").arg(target));

        const auto document = QJsonDocument::fromJson(file.readAll()).object();
        const auto versions = document.value(QStringLiteral("versions")).toArray();
        if (versions.isEmpty())
            return fail(tr("there is no %1 metadata here").arg(m_loaderName));

        QString chosen = m_request.neoforgeVersion;
        for (const auto& value : versions) {
            const auto entry = value.toObject();
            if (!chosen.isEmpty() && entry.value(QStringLiteral("version")).toString() != chosen)
                continue;

            // The index says which game version a release is for; anything else is not a candidate.
            bool forThisGame = chosen.isEmpty() ? false : true;
            for (const auto& requirement : entry.value(QStringLiteral("requires")).toArray()) {
                const auto object = requirement.toObject();
                if (object.value(QStringLiteral("uid")).toString() != QLatin1String("net.minecraft"))
                    continue;
                const QString wanted = object.value(QStringLiteral("equals")).toString();
                if (!wanted.isEmpty() && wanted != m_request.minecraftVersion) {
                    if (chosen.isEmpty())
                        continue;
                    forThisGame = false;
                    break;
                }
                forThisGame = true;
            }
            if (!forThisGame)
                continue;

            chosen = entry.value(QStringLiteral("version")).toString();
            break;
        }
        if (chosen.isEmpty())
            return fail(tr("%1 has no release for Minecraft %2").arg(m_loaderName, m_request.minecraftVersion));

        fetchNeoForgeComponent(chosen);
    });
}

void InstanceCreator::fetchNeoForgeComponent(const QString& version)
{
    emit status(tr("Reading the %1 profile").arg(m_loaderName));

    const QString url = m_loaderMetaBase + QStringLiteral("/net.neoforged/%1.json").arg(version);
    const QString target = QDir(m_request.dataRoot).filePath(QStringLiteral("tmp/neoforge-%1.json").arg(version));
    fetch(url, target, {}, [this, target, version] {
        QFile file(target);
        if (!file.open(QIODevice::ReadOnly))
            return fail(tr("could not read %1").arg(target));

        const auto document = QJsonDocument::fromJson(file.readAll()).object();
        if (document.value(QStringLiteral("mainClass")).toString().isEmpty())
            return fail(tr("the %1 metadata has no main class").arg(m_loaderName));

        // A label the launcher has never cached, so it does not try to replace this file with its own
        // on the way to opening the instance.
        m_loaderComponent = document;
        m_loaderComponent.insert(QStringLiteral("version"), version + QStringLiteral("-komorebi"));
        writeInstance();
    });
}

void InstanceCreator::installForge()
{
    emit status(tr("Installing the %1 loader").arg(m_loaderName));
    if (m_request.forgeVersion.isEmpty())
        return fail(tr("no Forge version was given"));

    QString java = m_request.javaPath;
    if (java.isEmpty()) {
        const auto majors = m_gameComponent.value(QStringLiteral("compatibleJavaMajors")).toArray();
        int required = majors.isEmpty() ? 17 : majors.first().toInt();
        if (required <= 0)
            required = 17;
        const auto chosen = Services::choose(Services::discover(m_request.dataRoot), required);
        if (!chosen)
            return fail(tr("no Java %1 was found to run the Forge installer").arg(required));
        java = chosen->path;
    }

    const QString name = QStringLiteral("%1-%2").arg(m_request.minecraftVersion, m_request.forgeVersion);
    const QString installerPath = QDir(m_request.dataRoot).filePath(QStringLiteral("tmp/forge-%1-installer.jar").arg(name));
    const QString url =
        QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/%1/forge-%1-installer.jar").arg(name);

    fetch(url, installerPath, {}, [this, installerPath, java] {
        const QString installRoot = QDir(m_request.dataRoot).filePath(QStringLiteral("tmp/forge-client"));
        QDir().mkpath(QDir(installRoot).filePath(QStringLiteral("versions")));

        QFile profiles(QDir(installRoot).filePath(QStringLiteral("launcher_profiles.json")));
        if (profiles.open(QIODevice::WriteOnly)) {
            profiles.write(QByteArrayLiteral("{\"profiles\":{}}"));
            profiles.close();
        }

        emit status(tr("Running the %1 installer").arg(m_loaderName));
        auto* process = new QProcess(this);
        process->setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                         [this, process, installRoot](int code, QProcess::ExitStatus) {
                             process->deleteLater();
                             if (!m_running)
                                 return;
                             if (code != 0)
                                 return fail(tr("the Forge installer exited with %1").arg(code));
                             readForgeInstall(installRoot);
                         });
        QObject::connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
            if (!m_running)
                return;
            process->deleteLater();
            fail(tr("the Forge installer could not be started"));
        });
        process->start(java, { QStringLiteral("-jar"), installerPath, QStringLiteral("--installClient"), installRoot });
    });
}

void InstanceCreator::copyInstallerLibraries(const QString& installRoot)
{
    const QDir source(QDir(installRoot).filePath(QStringLiteral("libraries")));
    const QDir target(QDir(m_request.dataRoot).filePath(QStringLiteral("libraries")));
    if (!source.exists())
        return;

    QDirIterator iterator(source.path(), QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QString destination = target.filePath(source.relativeFilePath(path));
        if (QFileInfo::exists(destination))
            continue;
        QDir().mkpath(QFileInfo(destination).absolutePath());
        QFile::copy(path, destination);
    }
}

void InstanceCreator::readForgeInstall(const QString& installRoot)
{
    const QString name = QStringLiteral("%1-forge-%2").arg(m_request.minecraftVersion, m_request.forgeVersion);
    QFile file(QDir(installRoot).filePath(QStringLiteral("versions/%1/%1.json").arg(name)));
    if (!file.open(QIODevice::ReadOnly))
        return fail(tr("the installer wrote no version description"));

    const auto document = QJsonDocument::fromJson(file.readAll());
    QString error;
    const auto component = Core::componentFromForgeVersion(document.object(), m_request.forgeVersion, &error);
    if (!component)
        return fail(error);

    // The installed description names the libraries it patched in; the jars carrying the patched game and
    // the loader itself sit beside them, and have to be named for the launch to find them.
    QJsonArray libraries = component->value(QStringLiteral("libraries")).toArray();
    const auto addArtifact = [this, &libraries, installRoot](const QString& classifier) {
        const QString relative = QStringLiteral("net/minecraftforge/forge/%1-%2/forge-%1-%2-%3.jar")
                                     .arg(m_request.minecraftVersion, m_request.forgeVersion, classifier);
        QFile artifact(QDir(installRoot).filePath(QStringLiteral("libraries/") + relative));
        if (!artifact.open(QIODevice::ReadOnly))
            return;
        const QByteArray bytes = artifact.readAll();
        artifact.close();

        const QJsonObject artifactInfo{
            { QStringLiteral("path"), relative },
            { QStringLiteral("url"), QStringLiteral("https://maven.minecraftforge.net/") + relative },
            { QStringLiteral("sha1"),
              QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex()) },
            { QStringLiteral("size"), bytes.size() }
        };
        libraries.append(
            QJsonObject{ { QStringLiteral("name"),
                           QStringLiteral("net.minecraftforge:forge:%1-forge-%2:%3")
                               .arg(m_request.minecraftVersion, m_request.forgeVersion, classifier) },
                         { QStringLiteral("downloads"), QJsonObject{ { QStringLiteral("artifact"), artifactInfo } } } });
    };
    addArtifact(QStringLiteral("client"));
    addArtifact(QStringLiteral("universal"));

    // The launcher's own metadata already describes this Forge release, in its own way, and it notices
    // when the file it has is not the one it remembers -- then it tries to fetch its copy before the
    // instance can be opened. A label it has never seen is an ordinary local component instead.
    m_loaderComponent = *component;
    m_loaderComponent.insert(QStringLiteral("version"),
                             m_request.forgeVersion + QStringLiteral("-komorebi"));
    m_loaderComponent.insert(QStringLiteral("libraries"), libraries);

    copyInstallerLibraries(installRoot);
    FS::deletePath(installRoot);
    writeInstance();
}

void InstanceCreator::writeComponent(const QString& uid, const QString& version, const QJsonObject& component,
                                     const QString& releaseTime, const QString& type, bool replaceExisting)
{
    const QString directory = QDir(m_versionsRoot).filePath(uid);
    if (!QDir().mkpath(directory)) {
        fail(tr("could not create %1").arg(directory));
        return;
    }

    const QString path = QDir(directory).filePath(version + QStringLiteral(".json"));
    if (!replaceExisting && QFileInfo::exists(path))
        return;

    const QByteArray bytes = QJsonDocument(component).toJson();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        fail(tr("could not write the %1 metadata").arg(uid));
        return;
    }
    file.write(bytes);
    file.close();

    // The launcher resolves a component through its index: a metadata file it cannot find listed there is
    // treated as absent, and the launcher then goes looking for a newer list somewhere else.
    const QString indexPath = QDir(directory).filePath(QStringLiteral("index.json"));
    QJsonObject index;
    QFile indexFile(indexPath);
    if (indexFile.open(QIODevice::ReadOnly))
        index = QJsonDocument::fromJson(indexFile.readAll()).object();
    indexFile.close();
    if (index.isEmpty()) {
        index = QJsonObject{ { QStringLiteral("formatVersion"), 1 },
                             { QStringLiteral("name"), component.value(QStringLiteral("name")) },
                             { QStringLiteral("uid"), uid } };
    }

    // A version the launcher already knows keeps its entry. Its index records a checksum for the file it
    // remembers, and rewriting the entry over a file this launcher wrote is what made it fetch its own
    // copy before the instance could be opened.
    for (const auto& value : index.value(QStringLiteral("versions")).toArray()) {
        if (value.toObject().value(QStringLiteral("version")).toString() == version) {
            QFile existing(indexPath);
            if (existing.exists())
                return;
        }
    }

    QJsonArray versions;
    for (const auto& value : index.value(QStringLiteral("versions")).toArray()) {
        if (value.toObject().value(QStringLiteral("version")).toString() != version)
            versions.append(value);
    }

    QJsonArray requirements;
    const auto requirement = component.value(QStringLiteral("requires")).toObject();
    if (!requirement.isEmpty())
        requirements.append(requirement);

    const QJsonObject entry{ { QStringLiteral("version"), version },
                             { QStringLiteral("type"), type },
                             { QStringLiteral("recommended"), false },
                             { QStringLiteral("releaseTime"), releaseTime },
                             { QStringLiteral("requires"), requirements },
                             { QStringLiteral("sha256"),
                               QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) } };
    versions.prepend(entry);
    index.insert(QStringLiteral("versions"), versions);

    QFile newIndex(indexPath);
    if (!newIndex.open(QIODevice::WriteOnly)) {
        fail(tr("could not write the %1 index").arg(uid));
        return;
    }
    newIndex.write(QJsonDocument(index).toJson());
    newIndex.close();
}
void InstanceCreator::fetchLoaderList()
{
    emit status(tr("Reading the %1 versions").arg(m_loaderName));

    const QString apiVersion = m_request.loader == QLatin1String("quilt") ? QStringLiteral("v3") : QStringLiteral("v2");
    const QString url = QStringLiteral("%1/%2/versions/loader/%3")
                            .arg(m_loaderMetaBase, apiVersion, m_request.minecraftVersion);
    const QString target = QDir(m_request.dataRoot).filePath(QStringLiteral("tmp/fabric-loader-list.json"));
    fetch(url, target, {}, [this, target] {
        QFile file(target);
        if (!file.open(QIODevice::ReadOnly))
            return fail(tr("could not read %1").arg(target));

        const auto document = QJsonDocument::fromJson(file.readAll());
        QString error;
        const auto releases = m_request.loader == QLatin1String("quilt")
                                  ? Core::parseQuiltLoaderList(document.array(), &error)
                                  : Core::parseFabricLoaderList(document.array(), &error);
        if (!releases)
            return fail(error);

        QString chosen = m_request.loaderVersion;
        if (chosen.isEmpty()) {
            for (const auto& release : *releases) {
                if (release.stable) {
                    chosen = release.version;
                    break;
                }
            }
            if (chosen.isEmpty())
                chosen = releases->first().version;
        } else {
            bool listed = false;
            for (const auto& release : *releases)
                listed = listed || release.version == chosen;
            if (!listed)
                return fail(tr("Fabric does not list loader %1 for Minecraft %2")
                                .arg(chosen, m_request.minecraftVersion));
        }
        fetchLoaderProfile(chosen);
    });
}

void InstanceCreator::fetchLoaderProfile(const QString& loaderVersion)
{
    emit status(tr("Reading the %1 profile").arg(m_loaderName));

    const QString apiVersion = m_request.loader == QLatin1String("quilt") ? QStringLiteral("v3") : QStringLiteral("v2");
    const QString url = QStringLiteral("%1/%2/versions/loader/%3/%4/profile/json")
                            .arg(m_loaderMetaBase, apiVersion, m_request.minecraftVersion, loaderVersion);
    const QString target = QDir(m_request.dataRoot).filePath(QStringLiteral("tmp/fabric-loader-profile.json"));
    fetch(url, target, {}, [this, target, loaderVersion] {
        QFile file(target);
        if (!file.open(QIODevice::ReadOnly))
            return fail(tr("could not read %1").arg(target));

        const auto document = QJsonDocument::fromJson(file.readAll());
        QString error;
        const auto component = Core::componentFromLoaderProfile(document.object(), m_loaderUid, m_loaderName, loaderVersion, &error);
        if (!component)
            return fail(error);
        m_loaderComponent = *component;
        writeInstance();
    });
}
void InstanceCreator::writeInstance()
{
    emit status(tr("Creating the instance"));

    // The launcher's own metadata may already describe this game version, and it splits the graphics
    // library into a component of its own. Keeping its file means keeping the instance's components the
    // way it resolves them; replacing it with this launcher's conversion would make it fetch its copy
    // before the instance could be opened.
    const QString existingGameMeta =
        QDir(m_versionsRoot).filePath(QStringLiteral("net.minecraft/") + m_versionId + QStringLiteral(".json"));
    const bool keepGameMeta = QFileInfo::exists(existingGameMeta);

    writeComponent(QStringLiteral("net.minecraft"), m_versionId, m_gameComponent, m_gameReleaseTime,
                   m_gameComponent.value(QStringLiteral("type")).toString(), /*replaceExisting=*/!keepGameMeta);
    if (!m_loaderComponent.isEmpty()) {
        writeComponent(m_loaderComponent.value(QStringLiteral("uid")).toString(),
                       m_loaderComponent.value(QStringLiteral("version")).toString(), m_loaderComponent,
                       QDateTime::currentDateTime().toString(Qt::ISODate), QStringLiteral("release"));
    }
    if (m_reported)
        return;

    m_assetIndexId = m_gameComponent.value(QStringLiteral("assetIndex")).toObject().value(QStringLiteral("id")).toString();

    const QDir instancesRoot(QDir(m_request.dataRoot).filePath(QStringLiteral("instances")));
    if (!QDir().mkpath(instancesRoot.path()))
        return fail(tr("could not create %1").arg(instancesRoot.path()));

    m_instanceDirectory = instancesRoot.filePath(uniqueInstanceId(instancesRoot, m_request.name));
    if (!QDir().mkpath(m_instanceDirectory))
        return fail(tr("could not create %1").arg(m_instanceDirectory));

    QJsonArray components;
    if (keepGameMeta) {
        // Whatever the game's own metadata asks for has to be in the instance as a component, or the
        // libraries it names will not be on the launch path at all.
        QFile gameFile(existingGameMeta);
        if (gameFile.open(QIODevice::ReadOnly)) {
            const auto game = QJsonDocument::fromJson(gameFile.readAll()).object();
            QJsonArray requirementList = game.value(QStringLiteral("requires")).toArray();
            if (game.value(QStringLiteral("requires")).isObject())
                requirementList.append(game.value(QStringLiteral("requires")));
            if (requirementList.isEmpty()) {
                QFile indexFile(QDir(m_versionsRoot).filePath(QStringLiteral("net.minecraft/index.json")));
                if (indexFile.open(QIODevice::ReadOnly)) {
                    const auto index = QJsonDocument::fromJson(indexFile.readAll()).object();
                    for (const auto& value : index.value(QStringLiteral("versions")).toArray()) {
                        const auto entry = value.toObject();
                        if (entry.value(QStringLiteral("version")).toString() == m_versionId) {
                            requirementList = entry.value(QStringLiteral("requires")).toArray();
                            break;
                        }
                    }
                }
            }
            for (const auto& requirement : requirementList) {
                    const auto object = requirement.toObject();
                    const QString uid = object.value(QStringLiteral("uid")).toString();
                    const QString version = object.value(QStringLiteral("suggests")).toString();
                    if (uid.isEmpty() || version.isEmpty() || uid == QLatin1String("net.minecraft"))
                        continue;
                if (!QFileInfo::exists(QDir(m_versionsRoot).filePath(uid + QLatin1Char('/') + version + QStringLiteral(".json"))))
                    continue;

                QString name = uid;
                QFile dependencyIndex(QDir(m_versionsRoot).filePath(uid + QStringLiteral("/index.json")));
                if (dependencyIndex.open(QIODevice::ReadOnly)) {
                    const auto document = QJsonDocument::fromJson(dependencyIndex.readAll()).object();
                    if (document.value(QStringLiteral("name")).isString())
                        name = document.value(QStringLiteral("name")).toString();
                }
                components.append(QJsonObject{ { QStringLiteral("uid"), uid },
                                               { QStringLiteral("version"), version },
                                               { QStringLiteral("cachedName"), name },
                                               { QStringLiteral("cachedVersion"), version },
                                               { QStringLiteral("cachedVolatile"), true },
                                               { QStringLiteral("dependencyOnly"), true } });
            }
        }
    }
    // The way the launcher writes these entries: a component it has already resolved is described by
    // what was resolved, and it only goes looking for metadata again when that is missing.
    components.append(QJsonObject{ { QStringLiteral("uid"), QStringLiteral("net.minecraft") },
                                   { QStringLiteral("version"), m_versionId },
                                   { QStringLiteral("cachedName"), QStringLiteral("Minecraft") },
                                   { QStringLiteral("cachedVersion"), m_versionId } });
    if (!m_loaderComponent.isEmpty()) {
        const QString loaderUid = m_loaderComponent.value(QStringLiteral("uid")).toString();
        const QString loaderVersion = m_loaderComponent.value(QStringLiteral("version")).toString();
        components.append(QJsonObject{ { QStringLiteral("uid"), loaderUid },
                                       { QStringLiteral("version"), loaderVersion },
                                       { QStringLiteral("cachedName"), m_loaderName },
                                       { QStringLiteral("cachedVersion"), loaderVersion } });
    }
    QJsonObject pack{
        { QStringLiteral("formatVersion"), 1 },
        { QStringLiteral("components"), components },
    };
    QFile packFile(QDir(m_instanceDirectory).filePath(QStringLiteral("mmc-pack.json")));
    if (!packFile.open(QIODevice::WriteOnly))
        return fail(tr("could not write mmc-pack.json into %1").arg(m_instanceDirectory));
    packFile.write(QJsonDocument(pack).toJson());
    packFile.close();

    QFile configFile(QDir(m_instanceDirectory).filePath(QStringLiteral("instance.cfg")));
    if (!configFile.open(QIODevice::WriteOnly))
        return fail(tr("could not write instance.cfg into %1").arg(m_instanceDirectory));
    configFile.write(QStringLiteral("[General]\nInstanceType=OneSix\nname=%1\n").arg(m_request.name).toUtf8());
    configFile.close();

    if (!m_request.ensureFiles)
        return finish();

    fetchLibraries();
}

void InstanceCreator::fetchLibraries()
{
    emit status(tr("Checking the files the instance needs"));

    QString error;
    auto files = missingInstanceFiles(m_request.dataRoot, m_instanceDirectory, &error);
    if (!error.isEmpty())
        return fail(error);

    // The index is named by the metadata, not listed among the libraries, so it is checked separately.
    QFile manifestFile(
        QDir(m_request.dataRoot).filePath(QStringLiteral("meta/net.minecraft/") + m_versionId + QStringLiteral(".json")));
    if (manifestFile.open(QIODevice::ReadOnly)) {
        const auto manifest = Core::VersionJson::parse(QJsonDocument::fromJson(manifestFile.readAll()).object(), &error);
        if (!manifest)
            return fail(error);
        files += missingAssetIndexFile(*manifest, m_request.dataRoot);
    }

    auto queue = new DownloadQueue(m_network, this);
    QObject::connect(queue, &DownloadQueue::progress, this, &InstanceCreator::progress);
    QObject::connect(queue, &DownloadQueue::finished, this, [this, queue](bool ok, const QStringList& failures) {
        queue->deleteLater();
        if (!m_running)
            return;
        if (!ok)
            return fail(failures.isEmpty() ? tr("the download failed") : failures.join(QStringLiteral("; ")));
        fetchAssets();
    });
    queue->start(files, m_request.source);
}

void InstanceCreator::fetchAssets()
{
    if (m_assetIndexId.isEmpty())
        return finish();

    QString error;
    const auto index = Core::loadAssetIndex(m_request.dataRoot, m_assetIndexId, &error);
    if (!index) {
        // An index this version does not actually use is not a reason to fail the creation.
        return finish();
    }

    emit status(tr("Checking the assets the game uses"));
    const auto files = missingAssetObjects(*index, m_request.dataRoot);
    if (files.isEmpty())
        return finish();

    auto queue = new DownloadQueue(m_network, this);
    QObject::connect(queue, &DownloadQueue::progress, this, &InstanceCreator::progress);
    QObject::connect(queue, &DownloadQueue::finished, this, [this, queue](bool ok, const QStringList& failures) {
        queue->deleteLater();
        if (!m_running)
            return;
        if (!ok)
            return fail(failures.isEmpty() ? tr("the asset download failed") : failures.join(QStringLiteral("; ")));
        finish();
    });
    queue->start(files, m_request.source);
}

}  // namespace Komorebi::Services
