// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "TopNavShell.h"

#include "ShellHelpers.h"

void TopNavShell::adopt(const WindowParts& parts)
{
    WindowShell::adoptColumns(parts, listWidth);

    WindowShell::placeToolBar(parts.window, parts.mainToolBar, Qt::TopToolBarArea);
    // A break, so the instance actions get a row of their own instead of being squeezed in beside
    // the global ones and losing most of their labels to the overflow menu.
    WindowShell::placeToolBar(parts.window, parts.instanceToolBar, Qt::TopToolBarArea, true);
}
