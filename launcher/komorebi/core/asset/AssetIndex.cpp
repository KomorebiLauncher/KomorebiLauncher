// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/asset/AssetIndex.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>

#include "komorebi/core/Support/ContainedPath.h"

namespace Komorebi::Core {

std::optional<AssetIndex> parseAssetIndex(const QJsonObject& document, const QString& id, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<AssetIndex> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    // The id ends up in a directory name (assets/virtual/<id>, assets/indexes/<id>.json), so it has to
    // be one name and not a path.
    if (!isPlainFileName(id))
        return fail(QStringLiteral("%1 is not an asset index name").arg(id));

    const auto objectsValue = document.value(QStringLiteral("objects"));
    if (!objectsValue.isObject())
        return fail(QStringLiteral("the asset index for %1 has no objects").arg(id));

    AssetIndex index;
    index.id = id;
    index.isVirtual = document.value(QStringLiteral("virtual")).toBool();
    index.mapToResources = document.value(QStringLiteral("map_to_resources")).toBool();

    const auto objects = objectsValue.toObject();
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        const auto object = it.value().toObject();
        AssetObject asset;
        asset.hash = object.value(QStringLiteral("hash")).toString();
        asset.size = static_cast<qint64>(object.value(QStringLiteral("size")).toDouble());
        if (asset.hash.isEmpty())
            continue;
        // The hash names the file inside assets/objects, two characters of it naming the folder: it is
        // forty hexadecimal digits or the index is not one this launcher can read.
        const bool hex = asset.hash.size() == 40
                         && std::all_of(asset.hash.cbegin(), asset.hash.cend(), [](QChar character) {
                                // The ASCII range only: the hash is also a path, and a digit from another
                                // script is not one of the characters the format writes.
                                return (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
                                       || (character >= QLatin1Char('a') && character <= QLatin1Char('f'));
                            });
        if (!hex)
            continue;
        index.objects.insert(it.key(), asset);
    }

    if (index.objects.isEmpty())
        return fail(QStringLiteral("the asset index for %1 lists no files").arg(id));
    return index;
}

std::optional<AssetIndex> loadAssetIndex(const QString& dataRoot, const QString& id, QString* error)
{
    if (!isPlainFileName(id)) {
        if (error)
            *error = QStringLiteral("%1 is not an asset index name").arg(id);
        return std::nullopt;
    }
    const QString path = QDir(dataRoot).filePath(QStringLiteral("assets/indexes/") + id + QStringLiteral(".json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("could not open %1").arg(path);
        return std::nullopt;
    }

    QJsonParseError parseError{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("%1 is not readable: %2").arg(path, parseError.errorString());
        return std::nullopt;
    }

    auto parsed = parseAssetIndex(document.object(), id, error);
    if (parsed && error)
        error->clear();
    return parsed;
}

}  // namespace Komorebi::Core
