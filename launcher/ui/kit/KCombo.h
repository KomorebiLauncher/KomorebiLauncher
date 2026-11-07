// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The kit's drop-down: the same paper and hairline as the field, with a drawn chevron instead of
 *  the platform arrow. The popup keeps the theme's list styling.
 */

#pragma once

#include <QComboBox>

class KCombo : public QComboBox {
    Q_OBJECT

   public:
    explicit KCombo(QWidget* parent = nullptr);

    QSize sizeHint() const override;

   protected:
    void paintEvent(QPaintEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

   private:
    /// True only when the focus arrived from the keyboard: the first widget of a page takes focus
    /// when the window opens, and a ring there reads as an error state.
    bool m_keyboardFocus = false;
};
