// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "RailShell.h"

#include "ShellHelpers.h"

void RailShell::adopt(const WindowParts& parts)
{
    WindowShell::adoptColumns(parts, railWidth);

    WindowShell::placeToolBar(parts.window, parts.mainToolBar, Qt::TopToolBarArea);
    WindowShell::placeToolBar(parts.window, parts.instanceToolBar, Qt::RightToolBarArea);
}
