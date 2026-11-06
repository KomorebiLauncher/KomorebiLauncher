// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace Domestic {

/**
 * The domestic launch-pack formats the launcher can read.
 *
 * Both are zip archives whose layout differs from the international ones: HMCL keeps a full
 * instance under `minecraft/` next to a `modpack.json`, while the MCBBS format keeps overrides
 * under `overrides/` and lists remote files in `mcbbs.packmeta`. Neither carries a loader field
 * in the same place, which is why detection here is by entry name and the actual interpretation
 * is left to the parsers below.
 */
enum class PackFormat {
    Unknown,
    HMCL,
    Mcbbs,
};

/** One file the pack wants from a remote service rather than from the archive itself. */
struct RemoteFile {
    QString path;
    QString sha1;
    qint64 size = 0;
    QString url;
    int curseforgeProjectId = 0;
    int curseforgeFileId = 0;
};

/** What both parsers produce, in the terms the instance creation code already understands. */
struct PackDescription {
    PackFormat format = PackFormat::Unknown;

    QString name;
    QString version;
    QString author;
    QString description;

    /** Minecraft version the pack was built for, empty when the manifest does not say. */
    QString gameVersion;
    /** Lower-case loader name as the launcher spells it: fabric, forge, neoforge, quilt; empty for vanilla. */
    QString loader;
    QString loaderVersion;

    /** Directory inside the archive that holds the game directory contents. */
    QString overridesPrefix;

    QList<RemoteFile> files;

    /** Problems that did not make the pack unreadable, in the user's terms. */
    QStringList warnings;
};

/** Which format an archive is, judged by the entry names it contains. */
auto detectFormat(const QStringList& entryNames) -> PackFormat;

/** Prefix the overrides of that format live under, with a trailing slash. */
auto overridesPrefixFor(PackFormat format) -> QString;

/**
 * Parses `mcbbs.packmeta`.
 *
 * The published description of this format and the implementations that exist disagree about
 * where the game version lives: it has appeared at the root, under `launch`, and under
 * `launchInfo`. All three are accepted, and the loader is taken from whichever of them carries
 * it. A pack whose version cannot be found anywhere is rejected with `error` set, because an
 * instance cannot be created without one.
 */
auto parseMcbbs(const QByteArray& manifestJson, PackDescription& out, QString* error) -> bool;

/**
 * Parses the pair of files an HMCL export carries: `modpack.json` for the pack's identity and
 * `minecraft/pack.json` for the instance itself.
 *
 * The loader is inferred from the Maven coordinates in `pack.json`'s library list, since the
 * manifest does not name it directly.
 */
auto parseHmcl(const QByteArray& modpackJson, const QByteArray& packJson, PackDescription& out, QString* error) -> bool;

/**
 * Reads a loader and its version out of Maven coordinates such as
 * `net.fabricmc:fabric-loader:0.15.11`.
 *
 * Returns false when the list contains no loader the launcher can install.
 */
auto loaderFromLibraries(const QStringList& mavenCoordinates, QString& loader, QString& version) -> bool;

}  // namespace Domestic
