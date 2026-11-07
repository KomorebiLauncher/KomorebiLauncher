// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The kit's slider: a hairline groove with the filled part in plum and a round handle on it.
 *  QSlider keeps the behaviour -- dragging, keys, page steps -- and only the drawing is ours.
 */

#pragma once

#include <QSlider>

class KSlider : public QSlider {
    Q_OBJECT

   public:
    explicit KSlider(Qt::Orientation orientation, QWidget* parent = nullptr);

   protected:
    void paintEvent(QPaintEvent* event) override;
};
