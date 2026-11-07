// SPDX-License-Identifier: GPL-3.0-only
#include "KField.h"

#include <QPainter>
#include <QPainterPath>

#include "KPalette.h"

KField::KField(QWidget* parent) : QLineEdit(parent)
{
    // The frame and the background are painted here, so the widget style must keep out of it.
    setFrame(false);
    setAttribute(Qt::WA_MacShowFocusRect, false);
    setTextMargins(10, 0, 10, 0);
}

void KField::paintEvent(QPaintEvent* event)
{
    const Komorebi::KColors colors = Komorebi::KColors::fromPalette(palette());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Komorebi::Radius::Control, Komorebi::Radius::Control);

    painter.fillPath(path, isEnabled() ? colors.paper : colors.paperDeep);
    painter.setPen(QPen(hasFocus() ? colors.plum : colors.line, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.end();

    // The text itself is still QLineEdit's job: the cursor, the selection and the input method all
    // live there, and re-implementing them would lose more than it gains.
    QLineEdit::paintEvent(event);
}
