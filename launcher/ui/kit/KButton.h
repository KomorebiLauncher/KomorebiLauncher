// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The kit's button: a hairline outline with the label, and exactly one filled button per page --
 *  the one that does the thing the page exists for.
 */

#pragma once

#include <QPushButton>

class KButton : public QPushButton {
    Q_OBJECT

   public:
    explicit KButton(const QString& text, QWidget* parent = nullptr);
    explicit KButton(QWidget* parent = nullptr);

    /// The filled variant. One per page at most; everything else stays outlined.
    void setPrimary(bool primary);
    bool isPrimary() const { return m_primary; }

    QSize sizeHint() const override;

   protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

   private:
    bool m_primary = false;
    bool m_hovered = false;
};
