// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Why an account operation did not go through, as a code rather than a sentence.
 *
 *  This layer is built and tested without a window, so it has no translator: a sentence written here
 *  could never be shown in the player's language. Callers own the words, and authErrorText() is the
 *  English one for logs and for the probe command line.
 */

#pragma once

#include <QString>

namespace Komorebi::Services {

enum class AuthError {
    None,
    VaultUnreadable,             ///< the accounts file exists but cannot be opened or parsed
    VaultUnwritable,             ///< the accounts file could not be written
    VaultSealFailed,             ///< the refresh token could not be sealed to this user
    OfflineNeedsMicrosoft,       ///< there is no Microsoft account for an offline name to stand beside
    OfflineNameMissing,          ///< an offline account was asked for without a name
    OfflineIsNotASignIn,         ///< a session made by offlineSession() was passed as a Microsoft sign-in
    LastMicrosoftBesideOffline,  ///< removing it would leave the offline accounts without a Microsoft one
    NoSuchAccount,               ///< nothing with that uuid is stored
};

/// English words for logs and for the probe command line. Never put this in front of a player.
inline QString authErrorText(AuthError error)
{
    switch (error) {
        case AuthError::None:
            return {};
        case AuthError::VaultUnreadable:
            return QStringLiteral("the accounts file could not be read");
        case AuthError::VaultUnwritable:
            return QStringLiteral("the accounts file could not be written");
        case AuthError::VaultSealFailed:
            return QStringLiteral("a refresh token could not be sealed to this user");
        case AuthError::OfflineNeedsMicrosoft:
            return QStringLiteral("an offline account is only allowed beside a Microsoft account");
        case AuthError::OfflineNameMissing:
            return QStringLiteral("an offline account needs a name");
        case AuthError::OfflineIsNotASignIn:
            return QStringLiteral("an offline name has to be added as an offline account");
        case AuthError::LastMicrosoftBesideOffline:
            return QStringLiteral("remove the offline accounts first: they need a Microsoft account to stand beside");
        case AuthError::NoSuchAccount:
            return QStringLiteral("no such account is stored");
    }
    return {};
}

}  // namespace Komorebi::Services
