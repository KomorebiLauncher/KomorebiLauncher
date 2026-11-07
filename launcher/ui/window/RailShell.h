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
 * The list reduced to a column of instance icons.
 *
 * Nothing else changes: the cards keep their plate and their spacing, and the delegate drops the
 * label on its own once a card is narrow enough. The column is what turns the list into a rail.
 */
class RailShell final : public IWindowShell {
   public:
    /** Width of the rail, in pixels: a card, its inset and the two margins. */
    static constexpr int railWidth = 76;

    void adopt(const WindowParts& parts) override;
    void saveState() const override {}
};
