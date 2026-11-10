// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The accounts the core launches with, and the single rule that governs them.
 *
 *  The rule is here rather than in the dialog that shows it: an offline name is only allowed beside a
 *  Microsoft sign-in, because playing the game needs a copy that somebody bought once. Keeping that in
 *  the core means a second entry point (a command line, a future page) cannot get around it, and it can
 *  be tested without a window.
 */

#pragma once

#include <QList>
#include <QString>

#include "komorebi/services/auth/AuthSession.h"
#include "komorebi/services/auth/TokenVault.h"

namespace Komorebi::Services {

class AccountStore {
   public:
    explicit AccountStore(TokenVault vault);

    /// The store inside a launcher data directory.
    static AccountStore forDataRoot(const QString& dataRoot);

    QString path() const { return m_vault.path(); }

    /// Every stored account. Microsoft ones come back without an access token until they are renewed.
    QList<AuthSession> accounts(QString* error = nullptr) const;

    /// True when there is a Microsoft account for an offline one to stand beside.
    bool canAddOffline(QString* error = nullptr) const;

    /// Stores a Microsoft sign-in (or a renewed copy of one).
    bool addMicrosoft(const AuthSession& session, QString* error = nullptr) const;

    /// Adds a name as an offline account, if canAddOffline(). The name is trimmed.
    bool addOffline(const QString& username, AuthSession* added = nullptr, QString* error = nullptr) const;

    /// Removes one account, refusing to leave an offline account without a Microsoft one.
    bool remove(const QString& uuid, QString* error = nullptr) const;

   private:
    TokenVault m_vault;
};

}  // namespace Komorebi::Services
