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

#include <QList>
#include <QString>
#include <QUrl>

namespace OptiFine {

/// The component id an installed OptiFine profile uses.
inline const QString componentUid = QStringLiteral("wtf.optifine");

/**
 * One OptiFine build as published by the mirror's version list.
 *
 * The list splits the edition across two fields and not consistently: type is "HD_U" for older
 * builds but "HD_U_K1" for newer ones, with patch then carrying "pre1". So the identifiers below
 * are always type + "_" + patch, which is what the installer itself names the produced version
 * with, and what the maven coordinate uses.
 */
struct Build {
    QString mcVersion;
    QString type;
    QString patch;
    QString filename;
    QString forgeNote;  /// "Forge N/A", "Forge #1902", or empty when the entry does not say
    bool hasForgeNote = false;
};

/** Parses the version list. Unusable entries are dropped rather than guessed around. */
auto parseBuilds(const QByteArray& json) -> QList<Build>;

/** Builds for one Minecraft version, newest patch last. */
auto buildsFor(const QList<Build>& builds, const QString& mcVersion) -> QList<Build>;

/** e.g. 1.13.2-OptiFine_HD_U_E7, matching the directory the installer writes. */
auto versionId(const Build& build) -> QString;

/** e.g. 1.13.2_HD_U_E7, the version part of the maven coordinate optifine:OptiFine:<that>. */
auto mavenVersion(const Build& build) -> QString;

/** The mirror's download endpoint for this build. */
auto downloadUrl(const QString& mirrorBase, const Build& build) -> QUrl;

/**
 * Turns the json OptiFine's installer wrote into an instance component.
 *
 * The produced json is plain Mojang format, so it needs the fields the component system keys on,
 * and the OptiFine artifact has to be pointed at a copy inside the instance because it has no
 * public maven home. Kept free of file and instance access so it can be tested directly.
 */
auto buildComponent(const QJsonObject& produced, const Build& build, const QString& jarUrl, const QString& jarSha1, qint64 jarSize)
    -> QJsonObject;

}  // namespace OptiFine
