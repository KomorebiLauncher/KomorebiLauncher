// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The accounts the core signs in with. They live beside the launcher's original ones rather than
 *  replacing them: a player who has been using the launcher keeps the account they already have, and
 *  signing in here is what starts a game through the new launch path with a core account.
 */

#pragma once

#include <QDialog>
#include <QList>

#include "komorebi/services/auth/AuthSession.h"

class QLabel;
class QListWidget;
class QPushButton;

class KomorebiAccountsDialog : public QDialog {
    Q_OBJECT

   public:
    explicit KomorebiAccountsDialog(QWidget* parent = nullptr);

   private:
    void reload();
    void signIn();
    void useSelected();
    void removeSelected();
    int selectedRow() const;
    QString storePath() const;

    QList<Komorebi::Services::AuthSession> m_accounts;

    QListWidget* m_list = nullptr;
    QLabel* m_hint = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_signIn = nullptr;
    QPushButton* m_use = nullptr;
    QPushButton* m_remove = nullptr;
};
