// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Komorebi Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "MirrorListEditor.h"

#include <QBrush>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

/** Same rule the single-mirror fields use: a real scheme and host, nothing else accepted. */
auto isUsablePrefix(const QString& text) -> bool
{
    const QUrl url(text.trimmed(), QUrl::TolerantMode);
    const auto scheme = url.scheme().toLower();
    return url.isValid() && !url.host().isEmpty() && (scheme == QLatin1String("https") || scheme == QLatin1String("http"));
}

auto makeItem(const QString& text, QListWidget* list, bool enabled) -> QListWidgetItem*
{
    auto item = new QListWidgetItem(text, list);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    return item;
}

}  // namespace

MirrorListEditor::MirrorListEditor(QWidget* parent) : QWidget(parent)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list);

    auto orderRow = new QHBoxLayout();
    m_upButton = new QPushButton(tr("Up"), this);
    m_downButton = new QPushButton(tr("Down"), this);
    m_removeButton = new QPushButton(tr("Remove"), this);
    orderRow->addWidget(m_upButton);
    orderRow->addWidget(m_downButton);
    orderRow->addWidget(m_removeButton);
    orderRow->addStretch();
    layout->addLayout(orderRow);

    auto addRow = new QHBoxLayout();
    m_newEntry = new QLineEdit(this);
    m_newEntry->setPlaceholderText(tr("https://example.com/maven"));
    m_addButton = new QPushButton(tr("Add"), this);
    addRow->addWidget(m_newEntry);
    addRow->addWidget(m_addButton);
    layout->addLayout(addRow);

    connect(m_list, &QListWidget::currentRowChanged, this, [this] { updateButtons(); });
    connect(m_upButton, &QPushButton::clicked, this, [this] { moveCurrent(-1); });
    connect(m_downButton, &QPushButton::clicked, this, [this] { moveCurrent(1); });
    connect(m_removeButton, &QPushButton::clicked, this, &MirrorListEditor::removeCurrent);
    connect(m_addButton, &QPushButton::clicked, this, [this] {
        auto text = m_newEntry->text().trimmed();
        if (!isUsablePrefix(text))
            return;
        makeItem(text, m_list, true);
        m_newEntry->clear();
    });
    connect(m_newEntry, &QLineEdit::textChanged, this, [addButton = m_addButton](const QString& text) {
        addButton->setEnabled(isUsablePrefix(text));
    });
    connect(m_newEntry, &QLineEdit::returnPressed, m_addButton, &QPushButton::click);

    m_addButton->setEnabled(false);
    updateButtons();
}

void MirrorListEditor::setEntries(const QString& multiline)
{
    m_list->clear();
    for (const auto& line : multiline.split(QLatin1Char('\n'))) {
        auto entry = line.trimmed();
        if (entry.isEmpty())
            continue;
        bool enabled = !entry.startsWith(QLatin1Char('#'));
        auto item = makeItem(enabled ? entry : entry.mid(1).trimmed(), m_list, enabled);

        // This value can be edited by hand in the configuration file, and an unusable entry is
        // skipped at download time; showing it as a working one would hide why it does nothing.
        if (!isUsablePrefix(item->text())) {
            item->setForeground(QBrush(Qt::red));
            item->setToolTip(tr("Not a usable http(s) address: this entry will be ignored."));
        }
    }
    updateButtons();
}

auto MirrorListEditor::entries() const -> QString
{
    QStringList lines;
    for (int i = 0; i < m_list->count(); i++) {
        auto item = m_list->item(i);
        auto text = item->text().trimmed();
        if (text.isEmpty())
            continue;
        lines.append(item->checkState() == Qt::Unchecked ? QStringLiteral("#") + text : text);
    }
    return lines.join(QLatin1Char('\n'));
}

void MirrorListEditor::moveCurrent(int offset)
{
    auto row = m_list->currentRow();
    auto target = row + offset;
    if (row < 0 || target < 0 || target >= m_list->count())
        return;

    auto item = m_list->takeItem(row);
    m_list->insertItem(target, item);
    m_list->setCurrentRow(target);
}

void MirrorListEditor::removeCurrent()
{
    if (m_list->currentRow() < 0)
        return;
    delete m_list->takeItem(m_list->currentRow());
    updateButtons();
}

void MirrorListEditor::updateButtons()
{
    auto row = m_list->currentRow();
    auto count = m_list->count();
    m_upButton->setEnabled(row > 0);
    m_downButton->setEnabled(row >= 0 && row + 1 < count);
    m_removeButton->setEnabled(row >= 0);
}
