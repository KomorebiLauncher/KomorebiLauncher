// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/auth/AuthSession.h"

#include <QCryptographicHash>
#include <QUuid>

namespace Komorebi::Services {

namespace {

/*
 * Java's UUID#nameUUIDFromBytes: an MD5 over the name, with the version and variant bits stamped in.
 * The game has used this derivation since the first offline mode, so a world written by the vanilla
 * launcher recognises the same name started from here.
 */
QString offlineUuid(const QString& username)
{
    QByteArray digest = QCryptographicHash::hash(QStringLiteral("OfflinePlayer:%1").arg(username).toUtf8(),
                                                 QCryptographicHash::Md5);
    digest[6] = static_cast<char>((static_cast<unsigned char>(digest[6]) & 0x0f) | 0x30);  // version 3
    digest[8] = static_cast<char>((static_cast<unsigned char>(digest[8]) & 0x3f) | 0x80);  // IETF variant
    return QUuid::fromRfc4122(digest).toString(QUuid::Id128);
}

}  // namespace

AuthSession offlineSession(const QString& username)
{
    AuthSession session;

    const QString name = username.trimmed();
    if (name.isEmpty())
        return session;

    session.offline = true;
    session.username = name;
    session.uuid = offlineUuid(name);
    // The game takes a token for every launch and ignores this one; the launcher's old offline
    // accounts used the same value, so nothing downstream has to special-case an empty token.
    session.accessToken = QStringLiteral("0");
    return session;
}

}  // namespace Komorebi::Services
