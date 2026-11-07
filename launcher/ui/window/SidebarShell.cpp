// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "SidebarShell.h"

#include <QHBoxLayout>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QWidget>

#include "MainContentPanel.h"

#include "ui/instanceview/InstanceView.h"

void SidebarShell::adopt(const WindowParts& parts)
{
    if (!parts.centralWidget || !parts.instanceView)
        return;

    // Ownership stays with the main window: the shell only decides where the widgets sit, which is
    // what makes a layout switch a re-parent rather than a rebuild.
    if (auto* layout = qobject_cast<QHBoxLayout*>(parts.centralWidget->layout())) {
        if (parts.instanceView->parentWidget() != parts.centralWidget)
            parts.instanceView->setParent(parts.centralWidget);

        // The list keeps a fixed column so the area beside it belongs to the selected instance.
        // Without it the cards stretched across the whole window and there was nowhere to put
        // anything next to them.
        parts.instanceView->setFixedWidth(SidebarShell::sidebarWidth);

        if (layout->indexOf(parts.instanceView) < 0)
            layout->addWidget(parts.instanceView);

        if (parts.contentPanel) {
            if (parts.contentPanel->parentWidget() != parts.centralWidget)
                parts.contentPanel->setParent(parts.centralWidget);
            if (layout->indexOf(parts.contentPanel) < 0)
                layout->addWidget(parts.contentPanel, 1);
        }
    }
}