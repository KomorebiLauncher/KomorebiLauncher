// SPDX-License-Identifier: GPL-3.0-only
#include "KCard.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include "KPalette.h"

KCard::KCard(QWidget* parent) : QFrame(parent)
{
    m_body = new QVBoxLayout(this);
    m_body->setContentsMargins(16, 14, 16, 14);
    m_body->setSpacing(12);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
}

void KCard::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    const Komorebi::KColors colors = Komorebi::KColors::fromPalette(palette());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Komorebi::Radius::Surface, Komorebi::Radius::Surface);

    // Slightly translucent, so the picture behind the window is still felt rather than covered.
    painter.fillPath(path, QColor(colors.paper.red(), colors.paper.green(), colors.paper.blue(), 190));
    painter.setPen(QPen(colors.line, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}
