// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QWidget>

class BaseInstance;
class QAction;
class QFrame;
class QHBoxLayout;
class QLabel;
class QToolButton;

/**
 * The area beside the instance list: which instance is selected, and the news.
 *
 * It holds no state of its own -- the window tells it what to show -- so a different layout can
 * take the same widget and place it elsewhere.
 */
class MainContentPanel final : public QWidget {
    Q_OBJECT

   public:
    explicit MainContentPanel(QWidget* parent = nullptr);

    /** Shows the selected instance, or nothing when the selection is empty. */
    void setInstance(BaseInstance* instance);

    /** Puts the news strip this panel should host. Takes ownership by re-parenting. */
    void setNewsWidget(QWidget* news);

    /** Adds a button that triggers the given action, for the "more news" entry. */
    void setMoreNewsAction(QAction* action);

   private:
    /** Mirrors the action's own visibility -- it hides itself while there is no news to open. */
    void updateMoreNewsVisibility();

    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QFrame* m_summaryCard = nullptr;
    QFrame* m_newsCard = nullptr;
    QHBoxLayout* m_newsRow = nullptr;
    QToolButton* m_moreNewsButton = nullptr;
};
