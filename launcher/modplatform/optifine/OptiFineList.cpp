// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "OptiFineList.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace OptiFine {

auto parseBuilds(const QByteArray& json) -> QList<Build>
{
    QList<Build> builds;
    QJsonParseError error{};
    auto doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return builds;

    for (const auto& value : doc.array()) {
        auto obj = value.toObject();
        Build build;
        build.mcVersion = obj.value("mcversion").toString();
        build.type = obj.value("type").toString();
        build.patch = obj.value("patch").toString();
        build.filename = obj.value("filename").toString();
        auto forge = obj.value("forge");
        build.hasForgeNote = forge.isString() && !forge.toString().isEmpty();
        build.forgeNote = build.hasForgeNote ? forge.toString() : QString();

        // Without all three the identifiers below would be built from holes.
        if (build.mcVersion.isEmpty() || build.type.isEmpty() || build.patch.isEmpty())
            continue;

        builds.append(build);
    }
    return builds;
}

auto buildsFor(const QList<Build>& builds, const QString& mcVersion) -> QList<Build>
{
    QList<Build> out;
    for (const auto& build : builds) {
        if (build.mcVersion == mcVersion)
            out.append(build);
    }
    return out;
}

auto versionId(const Build& build) -> QString
{
    return QStringLiteral("%1-OptiFine_%2_%3").arg(build.mcVersion, build.type, build.patch);
}

auto mavenVersion(const Build& build) -> QString
{
    return QStringLiteral("%1_%2_%3").arg(build.mcVersion, build.type, build.patch);
}

auto downloadUrl(const QString& mirrorBase, const Build& build) -> QUrl
{
    auto base = mirrorBase.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return QUrl(base + QStringLiteral("/optifine/%1/%2/%3").arg(build.mcVersion, build.type, build.patch));
}

auto mavenStoragePath(const QString& mavenName) -> QString
{
    auto parts = mavenName.split(QLatin1Char(':'));
    if (parts.size() < 3) {
        return QString();
    }
    auto group = parts.at(0);
    group.replace(QLatin1Char('.'), QLatin1Char('/'));
    auto const& artifact = parts.at(1);
    auto const& version = parts.at(2);
    return QStringLiteral("%1/%2/%3/%2-%4.jar").arg(group, artifact, version, version);
}

auto buildComponent(const QJsonObject& produced, const Build& build, const QList<LocalLibrary>& files) -> QJsonObject
{
    auto profile = produced;

    QJsonArray libraries;
    for (const auto& value : profile.value("libraries").toArray()) {
        auto lib = value.toObject();
        auto storage = mavenStoragePath(lib.value("name").toString());
        auto found = std::find_if(files.begin(), files.end(), [&](const LocalLibrary& file) {
            return !storage.isEmpty() && file.storage.compare(storage, Qt::CaseInsensitive) == 0;
        });
        if (found != files.end()) {
            lib["downloads"] = QJsonObject{ { "artifact",
                                             QJsonObject{ { "url", found->url },
                                                          { "sha1", found->sha1 },
                                                          { "size", static_cast<double>(found->size) } } } };
        }
        libraries.append(lib);
    }
    profile["libraries"] = libraries;

    profile["uid"] = componentUid;
    profile["name"] = QStringLiteral("OptiFine %1_%2").arg(build.type, build.patch);
    profile["version"] = build.patch;
    profile["requires"] = QJsonArray{ QJsonObject{ { "uid", QStringLiteral("net.minecraft") },
                                                   { "equals", build.mcVersion },
                                                   { "suggests", build.mcVersion } } };
    return profile;
}

}  // namespace OptiFine
