// SPDX-License-Identifier: GPL-3.0-only
#include "KSlider.h"

#include <QPainter>
#include <QPainterPath>

#include "KPalette.h"

namespace {

constexpr int kGrooveHeight = 4;
constexpr int kHandleDiameter = 14;

}  // namespace

KSlider::KSlider(Qt::Orientation orientation, QWidget* parent) : QSlider(orientation, parent)
{
    setCursor(Qt::PointingHandCursor);
}

void KSlider::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    const Komorebi::KColors colors = Komorebi::KColors::fromPalette(palette());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool horizontal = orientation() == Qt::Horizontal;
    const int length = horizontal ? width() : height();
    const int centre = horizontal ? height() / 2 : width() / 2;
    const int handle = kHandleDiameter;
    const int span = qMax(1, length - handle);

    const int range = maximum() - minimum();
    const double position = range > 0 ? double(value() - minimum()) / double(range) : 0.0;
    const int travel = int(position * span) + handle / 2;

    const QRect groove = horizontal ? QRect(0, centre - kGrooveHeight / 2, length, kGrooveHeight)
                                    : QRect(centre - kGrooveHeight / 2, 0, kGrooveHeight, length);
    const QRect filled = horizontal ? QRect(0, groove.top(), travel, kGrooveHeight)
                                    : QRect(groove.left(), groove.top(), kGrooveHeight, travel);

    painter.setPen(Qt::NoPen);
    painter.setBrush(colors.line);
    painter.drawRoundedRect(groove, kGrooveHeight / 2.0, kGrooveHeight / 2.0);

    painter.setBrush(isEnabled() ? colors.plum : colors.inkSoft);
    painter.drawRoundedRect(filled, kGrooveHeight / 2.0, kGrooveHeight / 2.0);

    const QPointF handleCentre = horizontal ? QPointF(travel, centre) : QPointF(centre, travel);
    const QRectF handleRect(handleCentre.x() - handle / 2.0, handleCentre.y() - handle / 2.0, handle, handle);
    painter.setBrush(colors.paper);
    painter.setPen(QPen(isEnabled() ? colors.plum : colors.inkSoft, 2.0));
    painter.drawEllipse(handleRect);
}
