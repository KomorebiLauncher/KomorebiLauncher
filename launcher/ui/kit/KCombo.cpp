// SPDX-License-Identifier: GPL-3.0-only
#include "KCombo.h"

#include <QAbstractItemView>
#include <QPainter>
#include <QPainterPath>

#include "KPalette.h"

KCombo::KCombo(QWidget* parent) : QComboBox(parent)
{
    setCursor(Qt::PointingHandCursor);
    // The theme hides the platform arrow and leaves the painting to this class; see the KCombo
    // rules in the theme stylesheet.
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
}

QSize KCombo::sizeHint() const
{
    const QSize base = QComboBox::sizeHint();
    return QSize(base.width() + 24, qMax(32, base.height() + 8));
}

void KCombo::paintEvent(QPaintEvent* event)
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

    // The chevron: two strokes, open when the list is showing.
    const QPoint at(rect().right() - 16, rect().center().y());
    QPainterPath chevron;
    const int half = 4, lift = 2;
    if (view() && view()->isVisible()) {
        chevron.moveTo(at.x() - half, at.y() + lift);
        chevron.lineTo(at.x(), at.y() - lift);
        chevron.lineTo(at.x() + half, at.y() + lift);
    } else {
        chevron.moveTo(at.x() - half, at.y() - lift);
        chevron.lineTo(at.x(), at.y() + lift);
        chevron.lineTo(at.x() + half, at.y() - lift);
    }
    painter.setPen(QPen(colors.ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(chevron);
    painter.end();

    QComboBox::paintEvent(event);
}
