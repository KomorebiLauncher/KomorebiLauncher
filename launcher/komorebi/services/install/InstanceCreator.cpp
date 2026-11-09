// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/install/InstanceCreator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QRegularExpression>

#include "komorebi/core/asset/AssetIndex.h"
#include "komorebi/core/version/VersionManifest.h"
#include "komorebi/services/download/AssetFiles.h"
#include "komorebi/services/download/DownloadQueue.h"
#include "komorebi/services/download/FileDownloader.h"
#include "komorebi/services/download/InstanceFiles.h"

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
    downloader->start(FileDownload{ QUrl(url), target, expected, -1 });
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
        writeInstance(*component);
    });
}

void InstanceCreator::writeInstance(const QJsonObject& component)
{
    emit status(tr("Creating the instance"));

    const QString metaDirectory = QDir(m_versionsRoot).filePath(QStringLiteral("net.minecraft"));
    if (!QDir().mkpath(metaDirectory))
        return fail(tr("could not create %1").arg(metaDirectory));

    const QString metaPath = QDir(metaDirectory).filePath(m_versionId + QStringLiteral(".json"));
    QFile metaFile(metaPath);
    if (!metaFile.open(QIODevice::WriteOnly))
        return fail(tr("could not write %1").arg(metaPath));
    metaFile.write(QJsonDocument(component).toJson());
    metaFile.close();

    m_assetIndexId = component.value(QStringLiteral("assetIndex")).toObject().value(QStringLiteral("id")).toString();

    const QDir instancesRoot(QDir(m_request.dataRoot).filePath(QStringLiteral("instances")));
    if (!QDir().mkpath(instancesRoot.path()))
        return fail(tr("could not create %1").arg(instancesRoot.path()));

    m_instanceDirectory = instancesRoot.filePath(uniqueInstanceId(instancesRoot, m_request.name));
    if (!QDir().mkpath(m_instanceDirectory))
        return fail(tr("could not create %1").arg(m_instanceDirectory));

    QJsonObject pack{
        { QStringLiteral("formatVersion"), 1 },
        { QStringLiteral("components"),
          QJsonArray{ QJsonObject{ { QStringLiteral("uid"), QStringLiteral("net.minecraft") },
                                   { QStringLiteral("version"), m_versionId } } } },
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
