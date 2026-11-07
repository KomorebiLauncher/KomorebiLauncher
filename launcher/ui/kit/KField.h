// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The kit's text field. QLineEdit keeps the editing behaviour (selection, IME, undo); only the
 *  surface is ours: paper, a hairline, and a plum edge while the field has focus.
 */

#pragma once

#include <QLineEdit>

class KField : public QLineEdit {
    Q_OBJECT

   public:
    explicit KField(QWidget* parent = nullptr);

   protected:
    void paintEvent(QPaintEvent* event) override;
};
