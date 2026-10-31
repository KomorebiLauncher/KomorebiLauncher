// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "AuthlibInjector.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace AuthlibInjector {

auto parseRelease(const QByteArray& json) -> Release
{
    Release release;
    QJsonParseError error{};
    auto doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return release;

    auto obj = doc.object();
    release.version = obj.value("version").toString();
    release.downloadUrl = obj.value("download_url").toString();
    release.sha256 = obj.value("checksums").toObject().value("sha256").toString();

    if (release.version.isEmpty() || release.downloadUrl.isEmpty() || release.sha256.isEmpty())
        return {};
    return release;
}

auto buildComponent(const Release& release, const QString& serverUrl, const QString& mcVersion, const QString& jarUrl,
                    const QString& jarSha1, qint64 jarSize) -> QJsonObject
{
    QJsonObject library{ { "name", mavenName + release.version },
                         { "downloads",
                           QJsonObject{ { "artifact",
                                          QJsonObject{ { "url", jarUrl },
                                                       { "sha1", jarSha1 },
                                                       { "size", static_cast<double>(jarSize) } } } } } };
    library["argument"] = serverUrl;

    QJsonObject component;
    component["formatVersion"] = 1;
    component["uid"] = componentUid;
    component["name"] = QStringLiteral("authlib-injector");
    component["version"] = release.version;
    component["+agents"] = QJsonArray{ library };

    if (!mcVersion.isEmpty()) {
        component["requires"] = QJsonArray{ QJsonObject{ { "uid", QStringLiteral("net.minecraft") },
                                                         { "equals", mcVersion },
                                                         { "suggests", mcVersion } } };
    }
    return component;
}

}  // namespace AuthlibInjector
