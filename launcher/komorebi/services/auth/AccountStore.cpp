// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/auth/AccountStore.h"

#include <utility>

namespace Komorebi::Services {

namespace {

bool hasMicrosoft(const QList<AuthSession>& accounts)
{
    for (const auto& account : accounts) {
        if (!account.offline)
            return true;
    }
    return false;
}

}  // namespace

AccountStore::AccountStore(TokenVault vault) : m_vault(std::move(vault))
{
}

AccountStore AccountStore::forDataRoot(const QString& dataRoot)
{
    return AccountStore(TokenVault::forDataRoot(dataRoot));
}

QList<AuthSession> AccountStore::accounts(QString* error) const
{
    return m_vault.load(error);
}

bool AccountStore::canAddOffline(QString* error) const
{
    if (hasMicrosoft(m_vault.load()))
        return true;
    if (error)
        *error = QStringLiteral("an offline account is only allowed beside a Microsoft account");
    return false;
}

bool AccountStore::addMicrosoft(const AuthSession& session, QString* error) const
{
    if (session.offline) {
        if (error)
            *error = QStringLiteral("an offline name has to be added as an offline account");
        return false;
    }
    return m_vault.upsert(session, error);
}

bool AccountStore::addOffline(const QString& username, AuthSession* added, QString* error) const
{
    if (!canAddOffline(error))
        return false;

    const AuthSession session = offlineSession(username);
    if (!session.isValid()) {
        if (error)
            *error = QStringLiteral("an offline account needs a name");
        return false;
    }
    if (!m_vault.upsert(session, error))
        return false;

    if (added)
        *added = session;
    return true;
}

bool AccountStore::remove(const QString& uuid, QString* error) const
{
    const QList<AuthSession> accounts = m_vault.load();

    bool found = false;
    bool removingMicrosoft = false;
    bool offlineRemains = false;
    int microsoftCount = 0;
    for (const auto& account : accounts) {
        if (account.uuid == uuid) {
            found = true;
            removingMicrosoft = !account.offline;
        }
        if (!account.offline && account.uuid != uuid)
            ++microsoftCount;
        if (account.offline && account.uuid != uuid)
            offlineRemains = true;
    }

    if (!found) {
        if (error)
            *error = QStringLiteral("no such account is stored");
        return false;
    }

    // Removing the last Microsoft account would leave the offline ones with nothing to stand beside.
    if (removingMicrosoft && microsoftCount == 0 && offlineRemains) {
        if (error)
            *error = QStringLiteral("remove the offline accounts first: they need a Microsoft account to stand beside");
        return false;
    }

    return m_vault.remove(uuid, error);
}

}  // namespace Komorebi::Services
