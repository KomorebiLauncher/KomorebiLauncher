// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "OptiFineInstallDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

OptiFineInstallDialog::OptiFineInstallDialog(QList<OptiFine::Build> builds, const QString& mcVersion, QWidget* parent)
    : QDialog(parent), m_builds(std::move(builds))
{
    setWindowTitle(tr("Install OptiFine for Minecraft %1").arg(mcVersion));
    setWindowModality(Qt::WindowModal);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Choose an OptiFine build:"), this));

    // The list arrives oldest first; people want the newest build at the top.
    std::reverse(m_builds.begin(), m_builds.end());

    m_list = new QListWidget(this);
    for (const auto& build : m_builds) {
        auto text = QStringLiteral("OptiFine %1 %2").arg(build.type, build.patch);
        if (build.filename.startsWith(QStringLiteral("preview_")))
            text += QStringLiteral(" (") + tr("preview") + QStringLiteral(")");
        if (build.hasForgeNote && build.forgeNote != QStringLiteral("Forge N/A"))
            text += QStringLiteral("  — ") + build.forgeNote;

        auto item = new QListWidgetItem(text, m_list);
        QString tooltip = build.filename;
        if (build.hasForgeNote)
            tooltip += QLatin1Char('\n') + tr("Installer note: %1").arg(build.forgeNote);
        item->setToolTip(tooltip);
    }
    layout->addWidget(m_list);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Install"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(m_list, &QListWidget::currentRowChanged, buttons->button(QDialogButtonBox::Ok),
            [buttons](int row) { buttons->button(QDialogButtonBox::Ok)->setEnabled(row >= 0); });
    layout->addWidget(buttons);

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(m_list->currentRow() >= 0);

    resize(420, 400);
}

auto OptiFineInstallDialog::selectedBuild() const -> const OptiFine::Build&
{
    return m_builds.at(m_list->currentRow());
}
