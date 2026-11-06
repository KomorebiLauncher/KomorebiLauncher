// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#pragma once

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "BaseInstance.h"
#include "ui/pages/BasePage.h"
#include "ui/widgets/MinecraftSettingsWidget.h"

class InstanceSettingsPage : public MinecraftSettingsWidget, public BasePage {
    Q_OBJECT

   public:
    explicit InstanceSettingsPage(MinecraftInstance* instance, QWidget* parent = nullptr) : MinecraftSettingsWidget(instance, parent)
    {
        connect(APPLICATION, &Application::globalSettingsAboutToOpen, this, &InstanceSettingsPage::apply);
        connect(APPLICATION, &Application::globalSettingsApplied, this, &InstanceSettingsPage::loadSettings);
        addAppearanceTab();
    }
    ~InstanceSettingsPage() override {}
    QString displayName() const override { return tr("Settings"); }
    QIcon icon() const override { return QIcon::fromTheme("instance-settings"); }
    QString id() const override { return "settings"; }
    bool apply() override
    {
        SettingsObject::Lock lock(m_instance->settings());
        saveSettings();
        return true;
    }
    QString helpPage() const override { return "Instance-settings"; }

   private slots:
    void chooseBackground()
    {
        const auto current = m_instance->settings()->get("InstanceBackground").toString();
        const auto chosen = QFileDialog::getOpenFileName(this, tr("Choose a background image"), QFileInfo(current).absolutePath(),
                                                         tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (chosen.isEmpty())
            return;
        setBackground(QDir::fromNativeSeparators(chosen));
    }

    void clearBackground() { setBackground({}); }

   private:
    /**
     * Builds the appearance tab instead of adding it to the .ui file.
     *
     * The settings widget is shared with the global settings dialog and is generated from a .ui
     * document; appending a tab in code keeps this fork's addition out of that file, so rebasing
     * on upstream does not turn into a conflict in generated XML.
     */
    void addAppearanceTab()
    {
        auto* tabs = findChild<QTabWidget*>("settingsTabs");
        if (!tabs)
            return;

        auto* page = new QWidget(tabs);
        auto* layout = new QVBoxLayout(page);

        auto* description = new QLabel(tr("An image shown behind the pages of this instance's window. "
                                          "The file is not copied: only its location is remembered, so moving or "
                                          "deleting it simply leaves the window plain again."),
                                       page);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_backgroundPath = new QLineEdit(page);
        m_backgroundPath->setReadOnly(true);
        m_backgroundPath->setPlaceholderText(tr("No background chosen"));
        layout->addWidget(m_backgroundPath);

        auto* buttons = new QWidget(page);
        auto* buttonLayout = new QHBoxLayout(buttons);
        auto* choose = new QPushButton(tr("Choose image"), buttons);
        auto* clear = new QPushButton(tr("Clear"), buttons);
        buttonLayout->addWidget(choose);
        buttonLayout->addWidget(clear);
        buttonLayout->addStretch(1);
        layout->addWidget(buttons);
        layout->addStretch(1);

        connect(choose, &QPushButton::clicked, this, &InstanceSettingsPage::chooseBackground);
        connect(clear, &QPushButton::clicked, this, &InstanceSettingsPage::clearBackground);

        tabs->addTab(page, tr("Appearance"));
        refreshBackgroundField();
    }

    void setBackground(const QString& path)
    {
        // Stored with forward slashes: instance.cfg is an INI file, where a single backslash starts
        // an escape sequence and would silently eat part of a Windows path.
        m_instance->settings()->set("InstanceBackground", path);
        refreshBackgroundField();
        // Applies the next time the instance window opens; the page is shown inside that window, so
        // repainting it while the player is still choosing would fight the dialog they are in.
    }

    void refreshBackgroundField()
    {
        if (!m_backgroundPath)
            return;
        const auto path = m_instance->settings()->get("InstanceBackground").toString();
        m_backgroundPath->setText(path);
        m_backgroundPath->setToolTip(path);
    }

    QLineEdit* m_backgroundPath = nullptr;
};
