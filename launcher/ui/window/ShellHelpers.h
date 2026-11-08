// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QHBoxLayout>
#include <QMainWindow>
#include <QToolBar>

#include "WindowParts.h"
#include "ui/instanceview/InstanceView.h"
#include "ui/window/MainContentPanel.h"

/**
 * The pieces every layout is built from.
 *
 * They live here rather than in one shell so the alternatives stay small: each one only differs in
 * the column width and in where the toolbars go.
 */
namespace WindowShell {

/**
 * The instance list in a fixed column, the selected instance filling the rest.
 *
 * Re-parents rather than rebuilds, which is what keeps the selection, the scroll position and the
 * models intact when the player switches layouts.
 */
inline void adoptColumns(const WindowParts& parts, int listWidth)
{
    QWidget* list = parts.instanceList ? parts.instanceList : static_cast<QWidget*>(parts.instanceView);
    if (!parts.centralWidget || !list)
        return;

    if (auto* layout = qobject_cast<QHBoxLayout*>(parts.centralWidget->layout())) {
        if (list->parentWidget() != parts.centralWidget)
            list->setParent(parts.centralWidget);

        list->setFixedWidth(listWidth);

        if (layout->indexOf(list) < 0)
            layout->addWidget(list);

        if (parts.contentPanel) {
            if (parts.contentPanel->parentWidget() != parts.centralWidget)
                parts.contentPanel->setParent(parts.centralWidget);
            if (layout->indexOf(parts.contentPanel) < 0)
                layout->addWidget(parts.contentPanel, 1);
        }
    }
}

/**
 * Puts a toolbar where this layout wants it, keeping whatever visibility it had.
 *
 * Adding a toolbar to an area shows it, which would undo a player hiding it from the view menu.
 * The toggle action is the record of that choice.
 */
inline void placeToolBar(QMainWindow* window, QToolBar* bar, Qt::ToolBarArea area, bool newRow = false)
{
    if (!window || !bar)
        return;

    auto* toggle = bar->toggleViewAction();
    const bool wanted = bar->isVisible() || (toggle && toggle->isChecked());

    // Only when it actually has to move: adding a break every time the layout is re-applied would
    // pile them up, and a toolbar dragged back and forth would collect an empty row each round trip.
    if (newRow && window->toolBarArea(bar) != area)
        window->addToolBarBreak(area);

    window->addToolBar(area, bar);
    bar->setVisible(wanted);
}

}  // namespace WindowShell
