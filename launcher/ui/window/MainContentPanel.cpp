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
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

#include "BaseInstance.h"

namespace {

/**
 * A rounded panel in the same visual language as the instance cards on the left.
 *
 * The fill comes from the theme, and a widget only draws a stylesheet background once this
 * attribute is on -- without it the card would stay transparent no matter what the sheet says.
 */
QFrame* makeCard(const QString& objectName, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(objectName);
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    return card;
}

}  // namespace

MainContentPanel::MainContentPanel(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(20, 16, 20, 16);
    outer->setSpacing(12);

    // What is selected.
    m_summaryCard = makeCard(QStringLiteral("contentCard"), this);
    auto* summaryLayout = new QVBoxLayout(m_summaryCard);
    summaryLayout->setContentsMargins(20, 18, 20, 18);
    summaryLayout->setSpacing(6);

    m_title = new QLabel(m_summaryCard);
    m_title->setObjectName(QStringLiteral("contentTitle"));
    m_subtitle = new QLabel(m_summaryCard);
    m_subtitle->setObjectName(QStringLiteral("contentSubtitle"));
    summaryLayout->addWidget(m_title);
    summaryLayout->addWidget(m_subtitle);

    // What is new: headline on the left, the way into the full list on the right.
    m_newsCard = makeCard(QStringLiteral("newsCard"), this);
    m_newsRow = new QHBoxLayout(m_newsCard);
    m_newsRow->setContentsMargins(20, 12, 20, 12);
    m_newsRow->setSpacing(12);

    outer->addWidget(m_summaryCard);
    outer->addWidget(m_newsCard);
    outer->addStretch(1);

    setInstance(nullptr);
}

void MainContentPanel::setNewsWidget(QWidget* news)
{
    if (!news || news->parentWidget() == m_newsCard)
        return;

    news->setParent(m_newsCard);
    // Left-aligned and only as wide as its text: a tool button that is given the whole row would
    // centre the headline in the middle of the card. The stretch takes the rest, so whatever is
    // added later (the "more news" action) ends up on the right edge.
    m_newsRow->insertWidget(0, news, 0, Qt::AlignLeft | Qt::AlignVCenter);
    m_newsRow->addStretch(1);
}

void MainContentPanel::setMoreNewsAction(QAction* action)
{
    if (!action || m_moreNewsButton)
        return;

    // QToolButton, not QPushButton: only the former can adopt an action and its text/icon/enabled
    // state, which is what this button is supposed to mirror.
    auto* button = new QToolButton(m_newsCard);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setObjectName(QStringLiteral("contentAction"));
    button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    // Added with an explicit alignment so it sits on the right edge, past the stretch that
    // setNewsWidget left behind.
    m_newsRow->addWidget(button, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_moreNewsButton = button;

    // The action hides itself while there is nothing to open. A standalone button does not track
    // that on its own, so it is mirrored here.
    updateMoreNewsVisibility();
    connect(action, &QAction::changed, this, &MainContentPanel::updateMoreNewsVisibility);
}

void MainContentPanel::updateMoreNewsVisibility()
{
    if (!m_moreNewsButton)
        return;

    if (const auto* action = m_moreNewsButton->defaultAction())
        m_moreNewsButton->setVisible(action->isVisible());
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
