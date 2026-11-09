// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The asset layouts older versions of the game expect.
 *
 *  Modern versions read the index and open assets/objects/<hash>. Before 1.7 the game looked files up
 *  by their names in a real directory instead, so an index can say that its objects also belong in
 *  `assets/virtual/<id>` (`virtual`) or in the instance's `resources/` folder (`map_to_resources`).
 *  The files have to be copied there; the game will not find them otherwise, and it does not fail when
 *  it cannot -- it starts without sounds, language or textures.
 */

#pragma once

#include <QString>

#include "komorebi/core/asset/AssetIndex.h"

namespace Komorebi::Core {

/**
 * The directory this index's files have to be copied into, or empty when it asks for none.
 *
 * `virtual` indexes go under the launcher's data directory, since the assets are shared; a
 * `map_to_resources` index asks for the instance's own resources folder.
 */
QString legacyAssetDirectory(const AssetIndex& index, const QString& dataRoot, const QString& instanceDirectory);

/**
 * Copies every object of the index to the path its name asks for.
 *
 * Files already there at the right size are left alone, so a second launch does not rewrite thousands
 * of small files. Returns false and fills `error` when a copy fails.
 */
bool materialiseLegacyAssets(const AssetIndex& index, const QString& dataRoot, const QString& instanceDirectory,
                             QString* error = nullptr);

}  // namespace Komorebi::Core
