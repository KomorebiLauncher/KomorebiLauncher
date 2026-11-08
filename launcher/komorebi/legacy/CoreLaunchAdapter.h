// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The one place where the core's launch path meets the launcher's own types.
 *
 *  The core is written against plain values; an instance and an account are the launcher's classes.
 *  This file is the join, and the only file in the new layer that includes them, so the day the old
 *  classes go away this is what has to be rewritten.
 */

#pragma once

#include <QString>

#include "minecraft/MinecraftInstance.h"
#include "minecraft/auth/MinecraftAccount.h"

namespace Komorebi::Legacy {

/// Starts the instance through the core with the given account. Returns false and fills `error`.
bool launchInstance(MinecraftInstance* instance, const MinecraftAccountPtr& account, QString* error = nullptr);

}  // namespace Komorebi::Legacy
