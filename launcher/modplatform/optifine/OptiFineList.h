// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Komorebi Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

namespace OptiFine {

/// The component id an installed OptiFine profile uses.
inline const QString componentUid = QStringLiteral("komorebi.optifine");

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

/** Where a maven name such as optifine:launchwrapper-of:2.3 lives below a maven repository root. */
auto mavenStoragePath(const QString& mavenName) -> QString;

/** A jar the installer left in its scratch tree, once it has been copied into the instance. */
struct LocalLibrary {
    QString storage;  /// the maven path it was found under, e.g. optifine/OptiFine/1.20.1_HD_U_I6/OptiFine-1.20.1_HD_U_I6.jar
    QString url;
    QString sha1;
    qint64 size = 0;
};

/**
 * Turns the json OptiFine's installer wrote into an instance component.
 *
 * The produced json is plain Mojang format, so it needs the fields the component system keys on. The libraries it
 * names have no public maven home - OptiFine ships its own fork of launchwrapper under optifine: and never
 * publishes it - so every one the installer left a file for is pointed at the copy inside the instance, and the
 * rest are left to resolve wherever they came from. Kept free of file and instance access so it can be tested
 * directly.
 */
auto buildComponent(const QJsonObject& produced, const Build& build, const QList<LocalLibrary>& files) -> QJsonObject;

}  // namespace OptiFine
