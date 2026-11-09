// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/asset/AssetLayout.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace Komorebi::Core {

namespace {

/// Where one object lives under assets/objects.
QString objectPath(const QString& dataRoot, const QString& hash)
{
    return QDir(dataRoot).filePath(QStringLiteral("assets/objects/") + hash.left(2) + QLatin1Char('/') + hash);
}

}  // namespace

QString legacyAssetDirectory(const AssetIndex& index, const QString& dataRoot, const QString& instanceDirectory)
{
    if (index.isVirtual)
        return QDir(dataRoot).filePath(QStringLiteral("assets/virtual/") + index.id);
    if (index.mapToResources)
        return QDir(instanceDirectory).filePath(QStringLiteral("resources"));
    return {};
}

bool materialiseLegacyAssets(const AssetIndex& index, const QString& dataRoot, const QString& instanceDirectory,
                             QString* error)
{
    const QString destination = legacyAssetDirectory(index, dataRoot, instanceDirectory);
    if (destination.isEmpty())
        return true;

    for (auto it = index.objects.begin(); it != index.objects.end(); ++it) {
        const auto& asset = it.value();
        if (asset.hash.isEmpty())
            continue;

        const QString source = objectPath(dataRoot, asset.hash);
        const QString target = QDir(destination).filePath(it.key());

        const QFileInfo existing(target);
        if (existing.isFile() && existing.size() == asset.size)
            continue;

        const QFileInfo sourceInfo(source);
        if (!sourceInfo.isFile()) {
            if (error)
                *error = QObject::tr("the asset %1 is missing from assets/objects").arg(asset.hash);
            return false;
        }

        if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
            if (error)
                *error = QObject::tr("could not create the folder for %1").arg(target);
            return false;
        }

        // A file of the wrong size is a half copy from an interrupted launch; the copy replaces it.
        if (existing.exists() && !QFile::remove(target)) {
            if (error)
                *error = QObject::tr("could not replace %1").arg(target);
            return false;
        }
        if (!QFile::copy(source, target)) {
            if (error)
                *error = QObject::tr("could not copy %1 to %2").arg(source, target);
            return false;
        }
    }
    return true;
}

}  // namespace Komorebi::Core
