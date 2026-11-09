// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Where signed-in accounts are kept.
 *
 *  Only the refresh token is persisted: an access token lives about a day, and keeping it would only
 *  give a stale copy of something that is asked for again at every start. On Windows the token is
 *  sealed to the current user with DPAPI, so another account on the same machine cannot read it by
 *  opening the file. Other platforms keep it encoded but not encrypted until a keychain backend is
 *  added -- the same trust level the launcher's existing accounts file has, and no worse.
 */

#pragma once

#include <QList>
#include <QString>

#include "komorebi/services/auth/AuthSession.h"

namespace Komorebi::Services {

class TokenVault {
   public:
    explicit TokenVault(QString path);

    /// The vault inside a launcher data directory.
    static TokenVault forDataRoot(const QString& dataRoot);

    QString path() const { return m_path; }

    /// Every stored account. Sessions come back without an access token; they are refreshed to get one.
    QList<AuthSession> load(QString* error = nullptr) const;

    bool save(const QList<AuthSession>& accounts, QString* error = nullptr) const;

    /// Adds the account, or replaces the entry with the same uuid.
    bool upsert(const AuthSession& account, QString* error = nullptr) const;

    bool remove(const QString& uuid, QString* error = nullptr) const;

   private:
    QString m_path;
};

}  // namespace Komorebi::Services
