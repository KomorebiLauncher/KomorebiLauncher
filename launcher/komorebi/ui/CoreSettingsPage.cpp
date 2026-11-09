// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/ui/CoreSettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Application.h"
#include "settings/SettingsObject.h"
#include "komorebi/services/download/DownloadSource.h"
#include "komorebi/ui/AccountsDialog.h"

CoreSettingsPage::CoreSettingsPage(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* launchGroup = new QGroupBox(this);
    auto* launchForm = new QFormLayout(launchGroup);

    m_useCoreLauncher = new QCheckBox(launchGroup);
    launchForm->addRow(QString(), m_useCoreLauncher);

    m_useCoreCreation = new QCheckBox(launchGroup);
    launchForm->addRow(QString(), m_useCoreCreation);

    m_launchLabel = new QLabel(launchGroup);
    m_launchLabel->setWordWrap(true);
    launchForm->addRow(m_launchLabel);

    m_creationLabel = new QLabel(launchGroup);
    m_creationLabel->setWordWrap(true);
    launchForm->addRow(m_creationLabel);

    m_useCoreAuth = new QCheckBox(launchGroup);
    launchForm->addRow(QString(), m_useCoreAuth);

    m_authLabel = new QLabel(launchGroup);
    m_authLabel->setWordWrap(true);
    launchForm->addRow(m_authLabel);

    auto* accountRow = new QHBoxLayout();
    m_manageAccounts = new QPushButton(launchGroup);
    accountRow->addWidget(m_manageAccounts);
    accountRow->addStretch(1);
    launchForm->addRow(QString(), accountRow);

    layout->addWidget(launchGroup);

    auto* downloadGroup = new QGroupBox(this);
    auto* downloadForm = new QFormLayout(downloadGroup);
    m_sourceLabel = new QLabel(downloadGroup);
    m_downloadSource = new QComboBox(downloadGroup);
    for (const auto& info : Komorebi::Services::downloadSources())
        m_downloadSource->addItem(info.name, info.id);
    downloadForm->addRow(m_sourceLabel, m_downloadSource);

    m_hint = new QLabel(downloadGroup);
    m_hint->setWordWrap(true);
    downloadForm->addRow(QString(), m_hint);
    layout->addWidget(downloadGroup);

    layout->addStretch(1);
    loadSettings();
    retranslate();

    connect(m_manageAccounts, &QPushButton::clicked, this, [this] {
        KomorebiAccountsDialog dialog(this);
        dialog.exec();
    });
}

CoreSettingsPage::~CoreSettingsPage() = default;

void CoreSettingsPage::loadSettings()
{
    m_useCoreLauncher->setChecked(APPLICATION->settings()->get("UseCoreLauncher").toBool());
    m_useCoreCreation->setChecked(APPLICATION->settings()->get("UseCoreInstanceCreation").toBool());
    m_useCoreAuth->setChecked(APPLICATION->settings()->get("UseCoreAuth").toBool());

    const QString source = APPLICATION->settings()->get("DownloadSource").toString();
    const int index = m_downloadSource->findData(source);
    m_downloadSource->setCurrentIndex(index >= 0 ? index : 0);
}

bool CoreSettingsPage::apply()
{
    APPLICATION->settings()->set("UseCoreLauncher", m_useCoreLauncher->isChecked());
    APPLICATION->settings()->set("UseCoreInstanceCreation", m_useCoreCreation->isChecked());
    APPLICATION->settings()->set("UseCoreAuth", m_useCoreAuth->isChecked());
    APPLICATION->settings()->set("DownloadSource", m_downloadSource->currentData().toString());
    return true;
}

void CoreSettingsPage::retranslate()
{
    m_useCoreLauncher->setText(tr("Start instances with the new core"));
    m_useCoreCreation->setText(tr("Create instances with the new wizard"));

    m_launchLabel->setText(
        tr("The core reads the instance's folder and fetches what the game is missing before starting it. "
           "Turning this off falls back to the launcher's original launch path."));
    m_creationLabel->setText(
        tr("The wizard asks for a name, a game version and, if wanted, a mod loader, and writes the instance "
           "through the core. Turning this off falls back to the launcher's original dialog."));

    m_useCoreAuth->setText(tr("Play as the Komorebi account"));
    m_authLabel->setText(
        tr("A Komorebi account starts the game through the core's own Microsoft sign-in. Turning this off, or "
           "having no account here, uses the launcher's original account."));
    m_manageAccounts->setText(tr("Manage accounts…"));

    m_sourceLabel->setText(tr("Downloads come from"));
    m_hint->setText(tr("Official uses the addresses the version manifests name. A mirror is used where it has "
                       "the file, with the original address kept as a fallback."));
}
