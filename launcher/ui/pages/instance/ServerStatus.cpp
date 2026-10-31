// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "ServerStatus.h"

#include <QJsonArray>
#include <QJsonObject>

namespace {
constexpr int maxComponentDepth = 16;

auto flattenComponent(const QJsonValue& value, int depth) -> QString
{
    if (depth > maxComponentDepth || value.isNull() || value.isUndefined()) {
        return {};
    }
    if (value.isString()) {
        return value.toString();
    }
    if (value.isArray()) {
        QString out;
        for (const auto& child : value.toArray()) {
            out += flattenComponent(child, depth + 1);
        }
        return out;
    }
    if (!value.isObject()) {
        return {};
    }

    auto object = value.toObject();
    QString out = object.value("text").toString();
    for (const auto& child : object.value("extra").toArray()) {
        out += flattenComponent(child, depth + 1);
    }
    if (out.isEmpty() && object.contains("translate")) {
        for (const auto& child : object.value("with").toArray()) {
            out += flattenComponent(child, depth + 1);
        }
    }
    return out;
}
}  // namespace

auto flattenMotd(const QJsonValue& value) -> QString
{
    return stripFormatCodes(flattenComponent(value, 0)).trimmed();
}

auto stripFormatCodes(const QString& text) -> QString
{
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); i++) {
        if (text.at(i) == u'§' && i + 1 < text.size()) {
            i++;
            continue;
        }
        out.append(text.at(i));
    }
    return out;
}

auto readServerStatus(const QJsonObject& data) -> ServerStatus
{
    ServerStatus status;
    // Getting here at all means the server answered; what it left out is unknown, not offline.
    status.answered = true;

    auto players = data.value("players").toObject();
    if (!players.isEmpty()) {
        status.online = static_cast<int>(players.value("online").toDouble(-1));
        status.max = static_cast<int>(players.value("max").toDouble(-1));
    }
    status.versionName = data.value("version").toObject().value("name").toString();
    status.motd = flattenMotd(data.value("description"));
    return status;
}
