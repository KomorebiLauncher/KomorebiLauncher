// SPDX-License-Identifier: GPL-3.0-only
#include "KCombo.h"

#include <QAbstractItemView>
#include <QFocusEvent>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>

#include "KPalette.h"

KCombo::KCombo(QWidget* parent) : QComboBox(parent)
{
    setCursor(Qt::PointingHandCursor);
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
}

QSize KCombo::sizeHint() const
{
    const QSize base = QComboBox::sizeHint();
    return QSize(base.width() + 32, qMax(32, base.height() + 8));
}

void KCombo::focusInEvent(QFocusEvent* event)
{
    switch (event->reason()) {
        case Qt::TabFocusReason:
        case Qt::BacktabFocusReason:
        case Qt::ShortcutFocusReason:
            m_keyboardFocus = true;
            break;
        default:
            m_keyboardFocus = false;
            break;
    }
    QComboBox::focusInEvent(event);
    update();
}

void KCombo::focusOutEvent(QFocusEvent* event)
{
    m_keyboardFocus = false;
    QComboBox::focusOutEvent(event);
    update();
}

void KCombo::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    const Komorebi::KColors colors = Komorebi::KColors::fromPalette(QApplication::palette());
    const bool open = view() && view()->isVisible();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Komorebi::Radius::Control, Komorebi::Radius::Control);

    painter.fillPath(path, isEnabled() ? colors.paper : colors.paperDeep);
    const bool ring = open || (m_keyboardFocus && hasFocus());
    painter.setPen(QPen(ring ? colors.plum : colors.line, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    // The label and the chevron are drawn here rather than by the widget style: with the theme's
    // border switched off the style has no padding of its own, and the text sat against the edge.
    const QRect textRect = rect().adjusted(12, 0, -30, 0);
    painter.setPen(isEnabled() ? colors.ink : colors.inkSoft);
    painter.setFont(font());
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                     fontMetrics().elidedText(currentText(), Qt::ElideRight, textRect.width()));

    const QPoint at(rect().right() - 16, rect().center().y());
    QPainterPath chevron;
    const int half = 4, lift = 2;
    if (open) {
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
}
