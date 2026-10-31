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

#include <QString>

namespace MSA {

/**
 * Whether a sign-in failure is about the application id we ship rather than about the player.
 *
 * The id this build uses is registered but not on Mojang's allow list, so Microsoft can refuse the
 * consent page and Minecraft can refuse the token exchange. Both read like account problems to
 * anyone not looking for them, and the advice differs: nothing on the player's side fixes an
 * application that was never approved.
 */
auto describesAppIdProblem(const QString& reason) -> bool;

}  // namespace MSA
