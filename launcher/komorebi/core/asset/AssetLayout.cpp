// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/asset/AssetLayout.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "komorebi/core/Support/ContainedPath.h"

namespace Komorebi::Core {

namespace {

/// Where one object lives under assets/objects.
QString objectPath(const QString& dataRoot, const QString& hash)
{
    // The hash is checked when the index is read; this is the second line of defence, because a path
    // that came out of a JSON document must not decide where a file is written.
    if (!isContainedRelativePath(hash) || hash.size() < 2)
        return {};
    return QDir(dataRoot).filePath(QStringLiteral("assets/objects/") + hash.left(2) + QLatin1Char('/') + hash);
}

}  // namespace

QString legacyAssetDirectory(const AssetIndex& index, const QString& dataRoot, const QString& instanceDirectory)
{
    if (index.isVirtual) {
        if (!isPlainFileName(index.id))
            return {};
        return QDir(dataRoot).filePath(QStringLiteral("assets/virtual/") + index.id);
    }
    if (index.mapToResources)
        return QDir(instanceDirectory).filePath(QStringLiteral("resources"));
    return {};
}

bool materialiseLegacyAssets(const AssetIndex& index, const QString& dataRoot, const QString& instanceDirectory,
                             QString* error)
{
    const QString destination = legacyAssetDirectory(index, dataRoot, instanceDirectory);
    if (destination.isEmpty()) {
        // An index that says it is virtual but names no usable directory is a broken index, not a
        // reason to keep going and let the game start without its sounds.
        if (index.isVirtual && error)
            *error = QStringLiteral("the asset index %1 does not name a directory").arg(index.id);
        return !index.isVirtual;
    }

    for (auto it = index.objects.begin(); it != index.objects.end(); ++it) {
        const auto& asset = it.value();
        if (asset.hash.isEmpty())
            continue;

        const QString source = objectPath(dataRoot, asset.hash);
        if (source.isEmpty()) {
            if (error)
                *error = QStringLiteral("the asset %1 does not name a file").arg(asset.hash);
            return false;
        }
        // The key is a path from the index ("minecraft/sounds/x.ogg") and has to stay below the
        // directory the assets are copied into.
        const QString target = containedPath(destination, it.key());
        if (target.isEmpty()) {
            if (error)
                *error = QStringLiteral("%1 does not name a file below the asset directory").arg(it.key());
            return false;
        }

        const QFileInfo existing(target);
        if (existing.isFile() && existing.size() == asset.size)
            continue;

        const QFileInfo sourceInfo(source);
        if (!sourceInfo.isFile()) {
            if (error)
                *error = QStringLiteral("the asset %1 is missing from assets/objects").arg(asset.hash);
            return false;
        }

        if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
            if (error)
                *error = QStringLiteral("could not create the folder for %1").arg(target);
            return false;
        }

        // A file of the wrong size is a half copy from an interrupted launch; the copy replaces it.
        if (existing.exists() && !QFile::remove(target)) {
            if (error)
                *error = QStringLiteral("could not replace %1").arg(target);
            return false;
        }
        if (!QFile::copy(source, target)) {
            if (error)
                *error = QStringLiteral("could not copy %1 to %2").arg(source, target);
            return false;
        }
    }
    return true;
}

}  // namespace Komorebi::Core
