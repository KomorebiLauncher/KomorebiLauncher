// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;
class QPushButton;

/**
 * An ordered list of mirror prefixes that can be reordered, switched off and extended.
 *
 * The value is a plain string with one entry per line and '#' commenting an entry out, so the
 * same setting can be edited in the configuration file and keeps its disabled entries instead of
 * losing them.
 */
class MirrorListEditor : public QWidget {
    Q_OBJECT

   public:
    explicit MirrorListEditor(QWidget* parent = nullptr);

    void setEntries(const QString& multiline);
    auto entries() const -> QString;

   private:
    void addEntry(const QString& text);
    void moveCurrent(int offset);
    void removeCurrent();
    void updateButtons();

    QListWidget* m_list;
    QLineEdit* m_newEntry;
    QPushButton* m_upButton;
    QPushButton* m_downButton;
    QPushButton* m_removeButton;
    QPushButton* m_addButton;
};
