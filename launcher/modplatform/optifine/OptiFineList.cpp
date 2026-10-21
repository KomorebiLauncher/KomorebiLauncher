// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "OptiFineList.h"

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

}  // namespace OptiFine
