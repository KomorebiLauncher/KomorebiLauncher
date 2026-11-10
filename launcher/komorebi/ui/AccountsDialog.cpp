// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/ui/AccountsDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "Application.h"
#include "settings/SettingsObject.h"
#include "komorebi/services/auth/AccountStore.h"
#include "komorebi/ui/DeviceCodeDialog.h"

using AccountStore = Komorebi::Services::AccountStore;
using AuthError = Komorebi::Services::AuthError;

KomorebiAccountsDialog::KomorebiAccountsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Komorebi accounts"));
    setMinimumWidth(520);
    setMinimumHeight(360);

    auto* layout = new QVBoxLayout(this);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_hint->setText(tr("Games started with the new launch path use the account marked as default. The "
                       "launcher's original accounts keep working alongside these. An offline account can "
                       "be added once a Microsoft account is here."));
    layout->addWidget(m_hint);

    m_list = new QListWidget(this);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setStyleSheet(QStringLiteral("QListWidget { background: rgba(250, 247, 240, 0.8);"
                                         " border: 1px solid #c9c2b4; border-radius: 6px; padding: 6px;"
                                         " outline: none; }"));
    layout->addWidget(m_list, 1);

    auto* actions = new QHBoxLayout();
    m_signIn = new QPushButton(tr("Sign in with Microsoft"), this);
    m_addOffline = new QPushButton(tr("Add offline account"), this);
    m_use = new QPushButton(tr("Use this account"), this);
    m_remove = new QPushButton(tr("Remove"), this);
    actions->addWidget(m_signIn);
    actions->addWidget(m_addOffline);
    actions->addStretch(1);
    actions->addWidget(m_use);
    actions->addWidget(m_remove);
    layout->addLayout(actions);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_signIn, &QPushButton::clicked, this, &KomorebiAccountsDialog::signIn);
    connect(m_addOffline, &QPushButton::clicked, this, &KomorebiAccountsDialog::addOffline);
    connect(m_use, &QPushButton::clicked, this, &KomorebiAccountsDialog::useSelected);
    connect(m_remove, &QPushButton::clicked, this, &KomorebiAccountsDialog::removeSelected);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        const bool has = selectedRow() >= 0;
        m_use->setEnabled(has);
        m_remove->setEnabled(has);
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, &KomorebiAccountsDialog::useSelected);

    reload();
}

QString KomorebiAccountsDialog::storePath() const
{
    return AccountStore::forDataRoot(APPLICATION->dataRoot()).path();
}

void KomorebiAccountsDialog::reload()
{
    const AccountStore store = AccountStore::forDataRoot(APPLICATION->dataRoot());
    AuthError error = AuthError::None;
    m_accounts = store.accounts(&error);

    const QString defaultAccount = APPLICATION->settings()->get("CoreAccount").toString();

    m_list->clear();
    for (const auto& account : m_accounts) {
        const bool isDefault = !defaultAccount.isEmpty() && account.uuid == defaultAccount;
        QString label = account.offline ? tr("%1  (offline)").arg(account.username) : account.username;
        if (isDefault)
            label += tr("  (default)");
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, account.uuid);
        item->setToolTip(account.uuid);
        m_list->addItem(item);
        if (isDefault)
            m_list->setCurrentItem(item);
    }

    const bool hasAccounts = !m_accounts.isEmpty();
    m_use->setEnabled(hasAccounts && selectedRow() >= 0);
    m_remove->setEnabled(hasAccounts && selectedRow() >= 0);
    m_list->setVisible(hasAccounts);

    // The button says why it is blocked instead of leaving the reason to a failed attempt.
    const bool offlineAllowed = store.canAddOffline();
    m_addOffline->setEnabled(offlineAllowed);
    m_addOffline->setToolTip(offlineAllowed
                                 ? tr("Play under a name that never signs in.")
                                 : tr("Sign in with Microsoft first: an offline name needs a Microsoft account beside it."));

    if (error != AuthError::None)
        m_status->setText(tr("The accounts file at %1 could not be read.").arg(storePath()));
    else if (!hasAccounts)
        m_status->setText(tr("No Komorebi accounts yet. Sign in with Microsoft to add one."));
    else
        m_status->setText(tr("%1 account(s), kept in %2").arg(m_accounts.size()).arg(storePath()));
}

int KomorebiAccountsDialog::selectedRow() const
{
    return m_list->currentRow();
}

void KomorebiAccountsDialog::signIn()
{
    KomorebiDeviceCodeDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const Komorebi::Services::AuthSession session = dialog.session();
    if (!session.isValid())
        return;

    AuthError error = AuthError::None;
    if (!AccountStore::forDataRoot(APPLICATION->dataRoot()).addMicrosoft(session, &error)) {
        m_status->setText(error == AuthError::VaultSealFailed
                              ? tr("The sign-in could not be kept safely on this computer.")
                              : tr("The account could not be saved at %1.").arg(storePath()));
        return;
    }

    // The account that was just signed in is the one the player wants to play with.
    APPLICATION->settings()->set("CoreAccount", session.uuid);
    reload();
    m_status->setText(tr("Signed in as %1. It is now the account used for launching.").arg(session.username));
}

void KomorebiAccountsDialog::addOffline()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(this, tr("Add offline account"), tr("Name to play as:"),
                                               QLineEdit::Normal, QString(), &accepted)
                             .trimmed();
    if (!accepted || name.isEmpty())
        return;

    AuthError error = AuthError::None;
    Komorebi::Services::AuthSession added;
    if (!AccountStore::forDataRoot(APPLICATION->dataRoot()).addOffline(name, &added, &error)) {
        switch (error) {
            case AuthError::OfflineNameMissing:
                m_status->setText(tr("An offline account needs a name."));
                break;
            case AuthError::OfflineNeedsMicrosoft:
                m_status->setText(tr("Sign in with Microsoft first: an offline name needs a Microsoft account beside it."));
                break;
            default:
                m_status->setText(tr("The offline account could not be saved at %1.").arg(storePath()));
                break;
        }
        reload();
        return;
    }

    APPLICATION->settings()->set("CoreAccount", added.uuid);
    reload();
    m_status->setText(tr("%1 will be used for launching.").arg(added.username));
}

void KomorebiAccountsDialog::useSelected()
{
    const int row = selectedRow();
    if (row < 0 || row >= m_accounts.size())
        return;

    APPLICATION->settings()->set("CoreAccount", m_accounts.at(row).uuid);
    reload();
    m_status->setText(tr("%1 will be used for launching.").arg(m_accounts.at(row).username));
}

void KomorebiAccountsDialog::removeSelected()
{
    const int row = selectedRow();
    if (row < 0 || row >= m_accounts.size())
        return;

    const Komorebi::Services::AuthSession account = m_accounts.at(row);
    if (QMessageBox::question(this, tr("Remove account"),
                              tr("Remove %1 from the launcher? The account itself is not affected.")
                                  .arg(account.username))
        != QMessageBox::Yes)
        return;

    AuthError error = AuthError::None;
    if (!AccountStore::forDataRoot(APPLICATION->dataRoot()).remove(account.uuid, &error)) {
        switch (error) {
            case AuthError::LastMicrosoftBesideOffline:
                m_status->setText(tr("Remove the offline accounts first: they need a Microsoft account to stand beside."));
                break;
            case AuthError::NoSuchAccount:
                m_status->setText(tr("That account is not stored any more."));
                break;
            default:
                m_status->setText(tr("The account could not be removed from %1.").arg(storePath()));
                break;
        }
        return;
    }

    if (APPLICATION->settings()->get("CoreAccount").toString() == account.uuid)
        APPLICATION->settings()->set("CoreAccount", QString());

    reload();
    m_status->setText(tr("Removed %1.").arg(account.username));
}
