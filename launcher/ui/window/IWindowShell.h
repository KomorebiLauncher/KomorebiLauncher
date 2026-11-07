// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include "WindowParts.h"

/**
 * A layout for the main window.
 *
 * Implementations only place the widgets they are given; the window keeps ownership and business
 * logic, so a shell can be swapped at runtime without recreating anything.
 */
class IWindowShell {
   public:
    virtual ~IWindowShell() = default;

    /** Places the given widgets in this shell's arrangement. Does not take ownership. */
    virtual void adopt(const WindowParts& parts) = 0;

    /** Remembers this shell's own sizes (docks, splits) so the next visit matches. */
    virtual void saveState() const = 0;
};
