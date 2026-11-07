// SPDX-License-Identifier: GPL-3.0-only
#include "KPalette.h"

namespace Komorebi {

static QColor mix(const QColor& a, const QColor& b, double t)
{
    const auto lerp = [t](int x, int y) { return int(x + (y - x) * t); };
    return QColor(lerp(a.red(), b.red()), lerp(a.green(), b.green()), lerp(a.blue(), b.blue()));
}

KColors KColors::fromPalette(const QPalette& palette)
{
    KColors colors;
    colors.paper = palette.color(QPalette::Window);
    colors.paperDeep = palette.color(QPalette::Button);
    colors.ink = palette.color(QPalette::WindowText);
    colors.plum = palette.color(QPalette::Link);

    // The two greys are derived, not declared: half way to the paper makes a quiet secondary text,
    // three quarters a hairline that holds on paper but never draws attention.
    colors.inkSoft = mix(colors.ink, colors.paper, 0.45);
    colors.line = mix(colors.ink, colors.paper, 0.78);
    colors.lineStrong = mix(colors.ink, colors.paper, 0.66);
    return colors;
}

}  // namespace Komorebi
