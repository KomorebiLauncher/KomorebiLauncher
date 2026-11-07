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
 * The layout the launcher ships with: a list along the left, the selected instance taking the
 * rest. This is the arrangement the window has always had, moved behind the shell interface so the
 * alternatives can be added next to it rather than instead of it.
 */
class QSpacerItem;

class SidebarShell final : public IWindowShell {
   public:
    /** Width of the instance column, in pixels. Matches the design mock-up. */
    static constexpr int sidebarWidth = 240;

    void adopt(const WindowParts& parts) override;
    void saveState() const override {}

   private:
    QSpacerItem* m_spacer = nullptr;
};