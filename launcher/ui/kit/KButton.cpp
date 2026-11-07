// SPDX-License-Identifier: GPL-3.0-only
#include "KButton.h"

#include <QFontMetrics>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>

#include "KPalette.h"

KButton::KButton(const QString& text, QWidget* parent) : QPushButton(text, parent)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
}

KButton::KButton(QWidget* parent) : KButton(QString(), parent) {}

void KButton::setPrimary(bool primary)
{
    if (m_primary == primary)
        return;
    m_primary = primary;
    update();
}

QSize KButton::sizeHint() const
{
    const QFontMetrics metrics = fontMetrics();
    return QSize(metrics.horizontalAdvance(text()) + 36, qMax(32, metrics.height() + 16));
}

void KButton::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QPushButton::enterEvent(event);
}

void KButton::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QPushButton::leaveEvent(event);
}

void KButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    const Komorebi::KColors colors = Komorebi::KColors::fromPalette(QApplication::palette());
    const bool enabled = isEnabled();
    const bool down = isDown();

    QColor fill = Qt::transparent;
    QColor border = colors.line;
    QColor textColour = colors.ink;

    if (m_primary) {
        fill = colors.plum;
        border = colors.plum;
        textColour = colors.paper;
        if (m_hovered && enabled)
            fill = colors.plum.lighter(112);
        if (down && enabled)
            fill = colors.plum.darker(115);
    } else if (m_hovered && enabled) {
        fill = colors.wash(26);
    } else if (down && enabled) {
        fill = colors.wash(46);
    }

    if (hasFocus())
        border = colors.plum;
    if (!enabled) {
        textColour = colors.inkSoft;
        border = colors.line;
        fill = Qt::transparent;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Komorebi::Radius::Control, Komorebi::Radius::Control);

    if (fill.alpha() > 0)
        painter.fillPath(path, fill);
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    painter.setPen(textColour);
    painter.setFont(font());
    painter.drawText(rect(), Qt::AlignCenter, text());
}
