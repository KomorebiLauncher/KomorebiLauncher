// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  What a signed-in account is, as far as the rest of the launcher is concerned.
 *
 *  The chain that produces one (device code, Microsoft token, Xbox Live, XSTS, Minecraft services) is
 *  a service of its own; everything above only ever sees this value.
 */

#pragma once

#include <QDateTime>
#include <QString>

namespace Komorebi::Services {

/// One sign-in, reduced to what starting a game needs.
struct AuthSession {
    QString username;         ///< the Minecraft name, as the game shows it
    QString uuid;             ///< the profile id, without dashes
    QString accessToken;      ///< Minecraft services token for this session
    QString refreshToken;     ///< what gets a new access token next time
    QString xuid;             ///< Xbox user id, which newer versions pass to the game
    QDateTime accessExpires;  ///< when the access token stops being accepted; invalid if unknown
    bool offline = false;     ///< a name that never signed in; only allowed beside a Microsoft account

    bool isValid() const { return !username.isEmpty() && !uuid.isEmpty() && !accessToken.isEmpty(); }

    /// True when the token is too old (or about to be) to be worth trying.
    bool expired() const { return accessExpires.isValid() && accessExpires <= QDateTime::currentDateTimeUtc(); }
};

/// The session of a name that never signed in.
///
/// The uuid is the one the game itself gives a name that was never verified, so a world remembers the
/// player. There is nothing to renew: the token is the placeholder the game ignores.
AuthSession offlineSession(const QString& username);

/// What the player is shown while signing in.
struct DeviceCode {
    QString deviceCode;       ///< the secret to poll the token endpoint with
    QString userCode;         ///< what the player types into the browser
    QString verificationUri;  ///< where they type it
    int interval = 5;         ///< seconds between polls
    int expiresIn = 900;      ///< seconds the code stays good for
};

}  // namespace Komorebi::Services
