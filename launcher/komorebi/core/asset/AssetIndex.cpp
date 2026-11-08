// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/asset/AssetIndex.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace Komorebi::Core {

std::optional<AssetIndex> parseAssetIndex(const QJsonObject& document, const QString& id, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<AssetIndex> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

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
        index.objects.insert(it.key(), asset);
    }

    if (index.objects.isEmpty())
        return fail(QStringLiteral("the asset index for %1 lists no files").arg(id));
    return index;
}

std::optional<AssetIndex> loadAssetIndex(const QString& dataRoot, const QString& id, QString* error)
{
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
