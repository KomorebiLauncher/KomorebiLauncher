// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "SidebarShell.h"

#include "ShellHelpers.h"

namespace {

/** The list in a column, the selected instance taking the rest, the actions on the right. */
void place(const WindowParts& parts, int listWidth)
{
    WindowShell::adoptColumns(parts, listWidth);

    WindowShell::placeToolBar(parts.window, parts.mainToolBar, Qt::TopToolBarArea);
    WindowShell::placeToolBar(parts.window, parts.instanceToolBar, Qt::RightToolBarArea);
}

}  // namespace

void SidebarShell::adopt(const WindowParts& parts)
{
    place(parts, sidebarWidth);
}
