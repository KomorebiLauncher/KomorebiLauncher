// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The launcher's own settings page.
 *
 *  The settings that decide whether an instance is started and created through the new core, and where
 *  downloads come from, had no window of their own; they were only reachable by editing the configuration
 *  file. This page is that window.
 */

#pragma once

#include <QWidget>

#include "ui/pages/BasePage.h"

class QCheckBox;
class QComboBox;
class QLabel;

class CoreSettingsPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit CoreSettingsPage(QWidget* parent = nullptr);
    ~CoreSettingsPage() override;

    QString id() const override { return QStringLiteral("komorebi-settings"); }
    QString displayName() const override { return tr("Komorebi"); }
    QIcon icon() const override { return QIcon::fromTheme(QStringLiteral("settings")); }
    bool apply() override;
    void retranslate() override;

   private:
    void loadSettings();

    QCheckBox* m_useCoreLauncher = nullptr;
    QCheckBox* m_useCoreCreation = nullptr;
    QComboBox* m_downloadSource = nullptr;
    QLabel* m_launchLabel = nullptr;
    QLabel* m_creationLabel = nullptr;
    QLabel* m_sourceLabel = nullptr;
    QLabel* m_hint = nullptr;
};
