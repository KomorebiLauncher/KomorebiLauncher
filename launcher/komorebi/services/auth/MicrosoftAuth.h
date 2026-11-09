// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Microsoft sign-in, the way a desktop launcher has to do it: a code the player types into a
 *  browser, then a chain of exchanges that ends with a Minecraft access token.
 *
 *      device code -> Microsoft token -> Xbox Live -> XSTS -> Minecraft services -> profile
 *
 *  Every step is one HTTPS request and this object reports each one; nothing here blocks, so a
 *  window can stay responsive while a player is still typing. The endpoints are settable so a test
 *  can point the whole chain at a server of its own.
 */

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

#include "komorebi/services/auth/AuthSession.h"

class QNetworkAccessManager;
class QNetworkReply;

namespace Komorebi::Services {

class MicrosoftAuth : public QObject {
    Q_OBJECT

   public:
    /// Where each step of the chain lives; the defaults are Microsoft's and the game services'.
    struct Endpoints {
        QUrl deviceCode;
        QUrl token;
        QUrl xboxUser;
        QUrl xsts;
        QUrl minecraftLogin;
        QUrl minecraftProfile;
    };

    explicit MicrosoftAuth(QNetworkAccessManager* network, QObject* parent = nullptr);
    ~MicrosoftAuth() override;

    /// The id the launcher is registered under; without one, signing in is impossible.
    void setClientId(const QString& clientId);
    QString clientId() const { return m_clientId; }

    void setEndpoints(const Endpoints& endpoints);
    Endpoints endpoints() const { return m_endpoints; }

    /// Asks Microsoft for a code to show the player; deviceCodeReady() follows.
    void requestDeviceCode();

    /// One poll of the token endpoint; pending(), slowDown(), signedIn() or failed() follows.
    void pollDeviceCode(const DeviceCode& code);

    /// Turns a stored refresh token into a session; signedIn() or failed() follows.
    void refresh(const QString& refreshToken);

    /// The code from the last requestDeviceCode(), for whoever is polling it.
    DeviceCode currentDeviceCode() const { return m_deviceCode; }

    /// The session from the last sign-in; valid once signedIn() has been emitted.
    AuthSession session() const { return m_session; }

    /// Gives up on the request in flight, if there is one.
    void cancel();

   signals:
    void deviceCodeReady();
    void pending();
    void slowDown();
    void signedIn();
    void failed(const QString& reason);

   private:
    using Answer = std::function<void(bool ok, int status, const QJsonObject& body, const QString& error)>;

    void send(QNetworkReply* reply, Answer answer);
    void postForm(const QUrl& url, const QByteArray& body, Answer answer);
    void postJson(const QUrl& url, const QJsonObject& body, Answer answer);
    void getJson(const QUrl& url, const QString& bearer, Answer answer);

    void exchangeXbox(const QString& microsoftToken, const QString& refreshToken, const QDateTime& expires);
    void requestXsts(const QString& xboxToken, const QString& refreshToken, const QDateTime& expires);
    void requestMinecraftToken(const QString& xstsToken, const QString& userHash, const QString& refreshToken,
                               const QDateTime& expires);
    void requestProfile(const QString& minecraftToken, const QString& refreshToken, const QDateTime& expires);

    QNetworkAccessManager* m_network = nullptr;
    QString m_clientId;
    Endpoints m_endpoints;
    DeviceCode m_deviceCode;
    AuthSession m_session;
    QNetworkReply* m_reply = nullptr;
};

}  // namespace Komorebi::Services
