// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The control kit's colours and metrics, taken from the palette the theme already set.
 *  Nothing here holds a colour of its own: the theme manifest is the one place a colour is
 *  written down, everything else derives from it.
 */

#pragma once

#include <QColor>
#include <QPalette>

namespace Komorebi {

namespace Radius {
constexpr int Chip = 2;
constexpr int Control = 4;
constexpr int Surface = 6;
}  // namespace Radius

struct KColors {
    QColor paper;      ///< the surface everything sits on
    QColor paperDeep;  ///< one step darker, for disabled and inset surfaces
    QColor ink;        ///< primary text
    QColor inkSoft;    ///< secondary text and placeholders
    QColor plum;       ///< the only accent colour
    QColor line;       ///< 1px hairline
    QColor lineStrong; ///< hairline for popups and separators

    /// Derives the kit's colours from the application palette, which the theme filled.
    static KColors fromPalette(const QPalette& palette);

    /// An accent wash for hover and selection: the plum at the given alpha.
    QColor wash(int alpha) const { return QColor(plum.red(), plum.green(), plum.blue(), alpha); }
};

}  // namespace Komorebi
