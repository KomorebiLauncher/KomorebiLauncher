// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/version/VersionChain.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

namespace Komorebi::Core {

namespace {

constexpr int kMaxChainLength = 10;

std::optional<VersionJson> readManifest(const QString& versionsRoot, const QString& versionId, QString* error)
{
    const QString path = QDir(versionsRoot).filePath(versionId + QLatin1Char('/') + versionId + QStringLiteral(".json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("no manifest for %1 at %2").arg(versionId, path);
        return std::nullopt;
    }

    QJsonParseError parseError{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("%1 is not a version manifest: %2").arg(path, parseError.errorString());
        return std::nullopt;
    }

    QString manifestError;
    auto manifest = VersionJson::parse(document.object(), &manifestError);
    if (!manifest) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, manifestError);
        return std::nullopt;
    }
    // A manifest may name itself differently from its directory; the caller asked for an id, and the
    // file that was found for it is what it gets.
    if (manifest->id.isEmpty())
        manifest->id = versionId;
    return manifest;
}

}  // namespace

std::optional<QList<VersionJson>> loadVersionChain(const QString& versionsRoot, const QString& versionId, QString* error)
{
    if (versionsRoot.isEmpty() || versionId.isEmpty()) {
        if (error)
            *error = QStringLiteral("no versions directory or version id was given");
        return std::nullopt;
    }

    QList<VersionJson> chain;
    QSet<QString> seen;
    QString current = versionId;

    while (!current.isEmpty()) {
        if (seen.contains(current)) {
            if (error)
                *error = QStringLiteral("%1 inherits from itself, directly or through a loop").arg(current);
            return std::nullopt;
        }
        if (chain.size() >= kMaxChainLength) {
            if (error)
                *error = QStringLiteral("the inheritance chain from %1 is longer than %2 links").arg(versionId).arg(kMaxChainLength);
            return std::nullopt;
        }
        seen.insert(current);

        auto manifest = readManifest(versionsRoot, current, error);
        if (!manifest)
            return std::nullopt;

        current = manifest->inheritsFrom;
        chain.append(std::move(*manifest));
    }

    return chain;
}

}  // namespace Komorebi::Core
