// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The list of asset objects a version uses.
 *
 *  The game asks for an asset by its name ("minecraft/sounds/...ogg") and the launcher's job is to
 *  know which file that is: the index maps every name to a hash, and the files themselves live under
 *  assets/objects by that hash. Two older layouts also copy them somewhere the game looks directly,
 *  which is what `virtual` and `map_to_resources` mean.
 */

#pragma once

#include <QHash>
#include <QString>

#include <optional>

#include "komorebi/core/version/VersionJson.h"

namespace Komorebi::Core {

struct AssetObject {
    QString hash;
    qint64 size = 0;
};

struct AssetIndex {
    QString id;
    QHash<QString, AssetObject> objects;  ///< asset name ("minecraft/sounds/...") to its file
    bool isVirtual = false;               ///< the game expects them copied into assets/virtual/<id>
    bool mapToResources = false;          ///< they belong in the instance's resources/ folder
};

/// Reads an index document (the shape of assets/indexes/<id>.json).
std::optional<AssetIndex> parseAssetIndex(const QJsonObject& document, const QString& id, QString* error = nullptr);

/// Reads assets/indexes/<id>.json from the launcher's data directory.
std::optional<AssetIndex> loadAssetIndex(const QString& dataRoot, const QString& id, QString* error = nullptr);

}  // namespace Komorebi::Core
