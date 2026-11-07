// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include "IWindowShell.h"

/**
 * The instance actions move out of the right-hand column and onto a bar of their own along the
 * top, under the global toolbar. The list keeps its column, so the target of those actions stays
 * where it was.
 *
 * Unlike the mock-up this does not split the window into modules: the launcher has no module
 * pages, so the only thing that can go into a top bar is the action set.
 */
class TopNavShell final : public IWindowShell {
   public:
    /** The list keeps the same column as the default layout. */
    static constexpr int listWidth = 240;

    void adopt(const WindowParts& parts) override;
    void saveState() const override {}
};
