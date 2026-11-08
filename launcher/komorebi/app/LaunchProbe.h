// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  A development hook: run the core's launch path for one version, without the window.
 *
 *  The point is comparison. The launcher's own launch path stays in place while the core is written,
 *  so the two have to be run against the same version directory and their results compared; this runs
 *  the core's, prints the command line it built, and echoes what the game says back.
 */

#pragma once

#include <QStringList>

namespace Komorebi::App {

/**
 * Runs the core against a data directory in the game's own layout and reports what happened.
 *
 *   --komorebi-data <dir>       required: holds versions/, libraries/ and assets/
 *   --komorebi-version <id>     required: which version under versions/ to start
 *   --komorebi-game-dir <dir>   optional: defaults to the version's own directory
 *   --komorebi-username <name>  optional: defaults to "Player"
 *   --komorebi-timeout <sec>    optional: stop the game after that long, 0 waits for ever
 *
 * Returns the game's exit code, 2 when the command line could not be assembled, 3 when nothing could
 * be printed at all.
 */
int probeLaunch(const QStringList& arguments);

}  // namespace Komorebi::App
