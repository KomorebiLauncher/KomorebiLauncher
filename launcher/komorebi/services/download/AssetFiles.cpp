// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/download/AssetFiles.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "komorebi/core/Support/ContainedPath.h"

namespace Komorebi::Services {

namespace {

/// Where the launcher keeps one asset object: assets/objects/<first two hex digits>/<hash>.
QString objectPath(const QString& dataRoot, const QString& hash)
{
    // The hash becomes two path segments, and it is read from an index that may have come from a server
    // that is not Mojang's; a value that is not a plain relative path is not a hash.
    if (!Core::isContainedRelativePath(hash) || hash.size() < 2)
        return {};
    return QDir(dataRoot).filePath(QStringLiteral("assets/objects/") + hash.left(2) + QLatin1Char('/') + hash);
}

bool fileMatches(const QString& path, const QByteArray& sha1, qint64 expectedSize)
{
    const QFileInfo info(path);
    if (!info.isFile())
        return false;
    if (!sha1.isEmpty()) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        QCryptographicHash hash(QCryptographicHash::Sha1);
        if (!hash.addData(&file))
            return false;
        return hash.result() == sha1;
    }
    return expectedSize < 0 || info.size() == expectedSize;
}

}  // namespace

QList<FileDownload> missingAssetIndexFile(const Core::VersionJson& manifest, const QString& dataRoot)
{
    const QString id = manifest.assetIndexId;
    // The id names the file the index is written to, and it comes from the manifest: a value that is not
    // a plain file name would put it outside the indexes folder.
    if (id.isEmpty() || !Core::isPlainFileName(id))
        return {};

    const QString target = QDir(dataRoot).filePath(QStringLiteral("assets/indexes/") + id + QStringLiteral(".json"));
    const auto sha1Hex = manifest.assetIndex.value(QStringLiteral("sha1")).toString();
    const qint64 size = static_cast<qint64>(manifest.assetIndex.value(QStringLiteral("size")).toDouble(-1));
    const QByteArray sha1 = QByteArray::fromHex(sha1Hex.toLatin1());
    if (fileMatches(target, sha1, size))
        return {};

    const QUrl url(manifest.assetIndex.value(QStringLiteral("url")).toString());
    if (!url.isValid())
        return {};
    return { FileDownload{ url, target, sha1, size } };
}

QList<FileDownload> missingAssetObjects(const Core::AssetIndex& index, const QString& dataRoot)
{
    QList<FileDownload> missing;
    for (auto it = index.objects.begin(); it != index.objects.end(); ++it) {
        const auto& asset = it.value();
        if (asset.hash.isEmpty())
            continue;

        const QString target = objectPath(dataRoot, asset.hash);
        if (target.isEmpty())
            continue;
        const QByteArray sha1 = QByteArray::fromHex(asset.hash.toLatin1());
        if (fileMatches(target, sha1, asset.size))
            continue;

        // The host is fixed by the format: the first two digits of the hash are also the folder.
        const QUrl url(QStringLiteral("https://resources.download.minecraft.net/") + asset.hash.left(2) + QLatin1Char('/')
                       + asset.hash);
        missing.append(FileDownload{ url, target, sha1, asset.size });
    }
    return missing;
}

}  // namespace Komorebi::Services
