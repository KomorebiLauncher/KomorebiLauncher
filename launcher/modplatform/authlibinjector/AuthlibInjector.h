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

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace AuthlibInjector {

/// The component id an installed agent is stored under.
inline const QString componentUid = QStringLiteral("wtf.authlib-injector");

/// The maven coordinate the jar is kept under inside the instance.
inline const QString mavenName = QStringLiteral("yushijinhun:authlib-injector:");

/** What the artifact index says about the current build. */
struct Release {
    QString version;
    QString downloadUrl;
    QString sha256;
};

/**
 * Reads the artifact index published by the mirror.
 *
 * Returns false unless every field the installer needs is present: a partial answer would install
 * an unverified or unnamed jar.
 */
auto parseRelease(const QByteArray& json) -> Release;

/**
 * The component that injects the agent.
 *
 * The agent carries its own library, so the jar is not listed under libraries as well - that would
 * also put it on the classpath. The server URL becomes the agent argument, which is how
 * authlib-injector is told where to send the client's authentication requests.
 */
auto buildComponent(const Release& release, const QString& serverUrl, const QString& mcVersion, const QString& jarUrl,
                    const QString& jarSha1, qint64 jarSize) -> QJsonObject;

/** The relative path the jar is stored at inside the instance's libraries folder. */
auto jarRelativePath(const Release& release) -> QString;

}  // namespace AuthlibInjector
