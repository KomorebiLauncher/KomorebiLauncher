// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

/** What a server's status answer says about it at the moment of the ping. */
struct ServerStatus {
    int online = -1;
    int max = -1;
    QString versionName;
    QString motd;
    int latencyMs = -1;
    /** Whether the server answered at all: a full answer may legitimately omit the players block. */
    bool answered = false;

    bool isOnline() const { return online >= 0; }
};

/** Reads the status document a server sent back. Whatever it left out stays at its default. */
auto readServerStatus(const QJsonObject& data) -> ServerStatus;

/** Turns a message of the day - plain text, or a chat component tree - into a single line of text. */
auto flattenMotd(const QJsonValue& value) -> QString;

/** Drops the section-sign codes that carry color and style, which is what a server list should not show. */
auto stripFormatCodes(const QString& text) -> QString;
