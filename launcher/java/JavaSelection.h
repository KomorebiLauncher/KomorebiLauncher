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

#include <QList>
#include <QString>

#include "java/JavaInstall.h"

namespace Java {

/**
 * Reads a user-written list of Java major versions, e.g. "8", "8-17", "8, 11, 17-21".
 *
 * Returns an empty list when the string is empty or unusable, which means "no restriction" -
 * a mistyped setting must not silently make Java unfindable.
 */
auto parseVersionList(const QString& spec) -> QList<int>;

auto isVersionAllowed(int major, const QList<int>& allowed) -> bool;

/**
 * The best detected installation that satisfies one of the versions the profile asks for and is
 * not excluded by the user's allow-list. Returns nullptr when there is no such installation.
 */
auto pickInstall(const QList<JavaInstallPtr>& installs, const QList<int>& requiredMajors, const QList<int>& allowed) -> JavaInstallPtr;

}  // namespace Java
