// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Loading a version and everything it inherits from disk.
 *
 *  The files are laid out the way the game's own launcher keeps them: one directory per version, the
 *  manifest named after the directory. A version that inherits names its parent in `inheritsFrom`, and
 *  the parent is looked up the same way; nothing else is consulted, so a chain loads from a plain
 *  directory of version folders without any metadata service.
 */

#pragma once

#include <QString>

#include <optional>

#include "komorebi/core/version/VersionJson.h"

namespace Komorebi::Core {

/**
 * The manifest of `versionId` and everything above it, child first.
 *
 * Fails on a manifest that is missing, unreadable, not a version, or inherits from something that is
 * not there -- naming the version that could not be loaded rather than returning a shorter chain.
 */
std::optional<QList<VersionJson>> loadVersionChain(const QString& versionsRoot, const QString& versionId, QString* error = nullptr);

}  // namespace Komorebi::Core
