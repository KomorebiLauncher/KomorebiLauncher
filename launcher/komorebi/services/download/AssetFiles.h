// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Which assets a version still needs on disk.
 *
 *  Two kinds of file stand between the launcher and a complete asset directory: the index itself,
 *  which the version's manifest names, and the objects it lists, which live under assets/objects by
 *  their hash. The game can start with some of them missing, but then it starts without sounds or
 *  with missing textures, and the launcher is the thing that was supposed to fetch them.
 */

#pragma once

#include <QList>
#include <QString>

#include "komorebi/core/asset/AssetIndex.h"
#include "komorebi/core/version/VersionJson.h"
#include "komorebi/services/download/FileDownloader.h"

namespace Komorebi::Services {

/// The index file, when `assets/indexes/<id>.json` is not already there.
QList<FileDownload> missingAssetIndexFile(const Core::VersionJson& manifest, const QString& dataRoot);

/// The objects the index lists that are not on disk under assets/objects.
QList<FileDownload> missingAssetObjects(const Core::AssetIndex& index, const QString& dataRoot);

}  // namespace Komorebi::Services
