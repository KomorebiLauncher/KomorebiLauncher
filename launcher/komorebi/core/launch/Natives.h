// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The native libraries the game needs unpacked beside it.
 *
 *  LWJGL and the audio stack ship their Windows, macOS and Linux code in jars of their own, and the
 *  game loads them from a directory rather than from the class path. The launcher's metadata names
 *  those jars like any other library, so this finds the ones that apply to the platform and unpacks
 *  them, leaving out what a jar's manifest and metadata say should not be copied.
 */

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "komorebi/core/version/VersionJson.h"

namespace Komorebi::Core {

/// One natives jar to unpack, with the entries the manifest says to leave out.
struct NativeJar {
    QString path;           ///< absolute path under libraries/
    QStringList exclude;    ///< entry prefixes to skip, beyond the META-INF the extractor always skips
};

/**
 * The natives jars the version lists for the platform.
 *
 * The launcher's component metadata names them like `org.lwjgl:lwjgl-natives-windows:3.3.1` with a
 * rule that limits them to one system, so the rule decides and the name only says what is inside.
 */
QList<NativeJar> nativeJars(const VersionJson& merged, const QString& librariesDirectory, const Platform& platform);

/**
 * Unpacks the jars into the directory the game is told about.
 *
 * The directory is left alone when a marker says these exact jars were already unpacked, so a second
 * launch does not rewrite the same files; a jar that was replaced is unpacked again.
 */
bool extractNatives(const QList<NativeJar>& jars, const QString& nativesDirectory, QString* error = nullptr);

}  // namespace Komorebi::Core
