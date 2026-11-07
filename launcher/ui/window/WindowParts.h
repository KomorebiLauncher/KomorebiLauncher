// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

class QStatusBar;
class QToolBar;
class QWidget;

class InstanceView;
class MainContentPanel;
class WideBar;

/**
 * The long-lived widgets of the main window, in one place.
 *
 * A layout shell only decides where these go; it never creates or destroys them. That keeps their
 * state -- selection, scroll position, models -- intact when the player switches layouts.
 */
struct WindowParts {
    QWidget* centralWidget = nullptr;
    InstanceView* instanceView = nullptr;
    WideBar* instanceToolBar = nullptr;
    QToolBar* mainToolBar = nullptr;
    QToolBar* newsToolBar = nullptr;
    MainContentPanel* contentPanel = nullptr;
    QWidget* newsWidget = nullptr;
    QStatusBar* statusBar = nullptr;
};