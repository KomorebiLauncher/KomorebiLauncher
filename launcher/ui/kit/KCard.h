// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The kit's card: paper, one hairline, a little more rounding than a control. Used for a grouped
 *  block of settings or for one instance in a list.
 */

#pragma once

#include <QFrame>

class QVBoxLayout;

class KCard : public QFrame {
    Q_OBJECT

   public:
    explicit KCard(QWidget* parent = nullptr);

    /// The layout a caller puts its rows into; the margins are already the card's padding.
    QVBoxLayout* body() const { return m_body; }

   protected:
    void paintEvent(QPaintEvent* event) override;

   private:
    QVBoxLayout* m_body = nullptr;
};
