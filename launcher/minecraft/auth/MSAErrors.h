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

#include <QString>

namespace MSA {

/**
 * Whether a sign-in failure is about the application id we ship rather than about the player.
 *
 * The id this build uses is registered but not authorized for Xbox Live, so Microsoft can refuse
 * the consent page, and a build can also point at an id that was deleted or that was registered as
 * a confidential client. All of those are things the player can work around by supplying an app id
 * of their own, which is different from "wrong password" and deserves different advice.
 */
auto describesAppIdProblem(const QString& reason) -> bool;

}  // namespace MSA
