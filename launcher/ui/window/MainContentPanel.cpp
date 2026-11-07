// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "MainContentPanel.h"

#include <QAction>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

#include "BaseInstance.h"

MainContentPanel::MainContentPanel(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(20, 16, 20, 16);
    outer->setSpacing(12);

    m_title = new QLabel(this);
    m_title->setObjectName("contentTitle");
    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName("contentSubtitle");

    outer->addWidget(m_title);
    outer->addWidget(m_subtitle);

    m_newsSlot = new QVBoxLayout();
    m_newsSlot->setContentsMargins(0, 8, 0, 0);
    m_newsSlot->setSpacing(6);
    outer->addLayout(m_newsSlot);

    m_actionsRow = new QVBoxLayout();
    m_actionsRow->setContentsMargins(0, 12, 0, 0);
    outer->addLayout(m_actionsRow);

    outer->addStretch(1);

    setInstance(nullptr);
}

void MainContentPanel::setNewsWidget(QWidget* news)
{
    if (!news || news->parentWidget() == this)
        return;

    news->setParent(this);
    m_newsSlot->addWidget(news);
}

void MainContentPanel::setMoreNewsAction(QAction* action)
{
    if (!action || !m_actionsRow)
        return;

    auto* button = new QPushButton(this);
    button->setDefaultAction(action);
    button->setObjectName("contentAction");
    button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_actionsRow->addWidget(button);
}

void MainContentPanel::setInstance(BaseInstance* instance)
{
    if (!instance) {
        m_title->setText(tr("No instance selected"));
        m_subtitle->setText(tr("Pick one on the left to get started."));
        return;
    }

    m_title->setText(instance->name());
    m_subtitle->setText(instance->getStatusbarDescription());
}