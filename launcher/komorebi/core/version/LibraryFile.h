// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Where a library's file actually sits under libraries/.
 *
 *  The manifest may say outright (artifact.path), and when it does not, the name gives it for ordinary
 *  libraries: group/artifact/version/artifact-version.jar. A natives jar is the exception -- its name
 *  carries the classifier as part of the artifact ("lwjgl-natives-windows"), while the file is named
 *  artifact-version-classifier.jar -- so the URL it names is the better guide. Deriving one from the
 *  name and the other from the URL puts the download and the unpack in different places, which is how
 *  a launch ends up complaining that a jar it just fetched is missing.
 *
 *  Every path here is checked against the directory it is claimed to be below: the strings come out
 *  of a manifest, not out of this launcher.
 */

#pragma once

#include <QString>

#include "komorebi/core/version/VersionJson.h"

namespace Komorebi::Core {

/// The file under librariesDirectory this entry names.
QString libraryFilePath(const LibraryEntry& entry, const QString& librariesDirectory);

/// The natives file the entry names for the platform, or empty when it names none.
QString nativeLibraryPath(const LibraryEntry& entry, const Platform& platform, const QString& librariesDirectory);

/// The classifier download the entry names for the platform, or nullptr when it names none.
const ClassifierFile* classifierFile(const LibraryEntry& entry, const Platform& platform);

}  // namespace Komorebi::Core
