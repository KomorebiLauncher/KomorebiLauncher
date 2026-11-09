// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/auth/MicrosoftAuth.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace Komorebi::Services {

namespace {

constexpr auto kScope = "XboxLive.signin offline_access";

QByteArray form(const QList<QPair<QString, QString>>& fields)
{
    QUrlQuery query;
    for (const auto& field : fields)
        query.addQueryItem(field.first, field.second);
    return query.toString(QUrl::FullyEncoded).toUtf8();
}

QString errorText(const QJsonObject& body, const QString& fallback)
{
    const QString description = body.value(QStringLiteral("error_description")).toString();
    if (!description.isEmpty())
        return description;
    const QString message = body.value(QStringLiteral("errorMessage")).toString();
    if (!message.isEmpty())
        return message;
    const QString shortName = body.value(QStringLiteral("error")).toString();
    if (!shortName.isEmpty())
        return shortName;
    return fallback;
}

}  // namespace

MicrosoftAuth::MicrosoftAuth(QNetworkAccessManager* network, QObject* parent) : QObject(parent), m_network(network)
{
    m_endpoints.deviceCode = QUrl(QStringLiteral("https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode"));
    m_endpoints.token = QUrl(QStringLiteral("https://login.microsoftonline.com/consumers/oauth2/v2.0/token"));
    m_endpoints.xboxUser = QUrl(QStringLiteral("https://user.auth.xboxlive.com/user/authenticate"));
    m_endpoints.xsts = QUrl(QStringLiteral("https://xsts.auth.xboxlive.com/xsts/authorize"));
    m_endpoints.minecraftLogin = QUrl(QStringLiteral("https://api.minecraftservices.com/authentication/login_with_xbox"));
    m_endpoints.minecraftProfile = QUrl(QStringLiteral("https://api.minecraftservices.com/minecraft/profile"));
}

MicrosoftAuth::~MicrosoftAuth() = default;

void MicrosoftAuth::setClientId(const QString& clientId)
{
    m_clientId = clientId;
}

void MicrosoftAuth::setEndpoints(const Endpoints& endpoints)
{
    m_endpoints = endpoints;
}

void MicrosoftAuth::cancel()
{
    if (!m_reply)
        return;
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    reply->abort();
    reply->deleteLater();
}

void MicrosoftAuth::send(QNetworkReply* reply, Answer answer)
{
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, answer = std::move(answer)] {
        if (m_reply == reply)
            m_reply = nullptr;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();
        const QString networkError = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        reply->deleteLater();

        QJsonObject body;
        if (!payload.isEmpty()) {
            const QJsonDocument document = QJsonDocument::fromJson(payload);
            if (document.isObject())
                body = document.object();
        }
        const bool ok = networkError.isEmpty() && status >= 200 && status < 300;
        answer(ok, status, body, networkError);
    });
}

void MicrosoftAuth::postForm(const QUrl& url, const QByteArray& body, Answer answer)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    send(m_network->post(request, body), std::move(answer));
}

void MicrosoftAuth::postJson(const QUrl& url, const QJsonObject& body, Answer answer)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    send(m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact)), std::move(answer));
}

void MicrosoftAuth::getJson(const QUrl& url, const QString& bearer, Answer answer)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + bearer.toUtf8());
    request.setRawHeader("Accept", "application/json");
    send(m_network->get(request), std::move(answer));
}

void MicrosoftAuth::requestDeviceCode()
{
    if (m_clientId.isEmpty()) {
        emit failed(tr("no Microsoft app id is configured"));
        return;
    }

    const QByteArray body = form({ { QStringLiteral("client_id"), m_clientId },
                                   { QStringLiteral("scope"), QString::fromLatin1(kScope) } });
    postForm(m_endpoints.deviceCode, body, [this](bool ok, int, const QJsonObject& object, const QString& error) {
        if (!ok) {
            emit failed(tr("Microsoft would not start a sign-in: %1").arg(errorText(object, error)));
            return;
        }

        m_deviceCode.deviceCode = object.value(QStringLiteral("device_code")).toString();
        m_deviceCode.userCode = object.value(QStringLiteral("user_code")).toString();
        m_deviceCode.verificationUri = object.value(QStringLiteral("verification_uri")).toString();
        m_deviceCode.interval = qMax(1, object.value(QStringLiteral("interval")).toInt(5));
        m_deviceCode.expiresIn = object.value(QStringLiteral("expires_in")).toInt(900);
        if (m_deviceCode.deviceCode.isEmpty() || m_deviceCode.userCode.isEmpty()) {
            emit failed(tr("Microsoft returned no device code"));
            return;
        }
        emit deviceCodeReady();
    });
}

void MicrosoftAuth::pollDeviceCode(const DeviceCode& code)
{
    if (m_clientId.isEmpty()) {
        emit failed(tr("no Microsoft app id is configured"));
        return;
    }

    const QByteArray body = form({ { QStringLiteral("grant_type"), QStringLiteral("urn:ietf:params:oauth:grant-type:device_code") },
                                   { QStringLiteral("client_id"), m_clientId },
                                   { QStringLiteral("device_code"), code.deviceCode } });
    postForm(m_endpoints.token, body, [this](bool ok, int, const QJsonObject& object, const QString& error) {
        if (!ok) {
            // While the player is still in the browser Microsoft answers 400 + authorization_pending,
            // which is not a failure: it is the expected answer until they press approve.
            const QString state = object.value(QStringLiteral("error")).toString();
            if (state == QLatin1String("authorization_pending")) {
                emit pending();
                return;
            }
            if (state == QLatin1String("slow_down")) {
                emit slowDown();
                return;
            }
            emit failed(tr("sign-in failed: %1").arg(errorText(object, error)));
            return;
        }

        const QString accessToken = object.value(QStringLiteral("access_token")).toString();
        if (accessToken.isEmpty()) {
            emit failed(tr("Microsoft returned no access token"));
            return;
        }
        const QDateTime expires = QDateTime::currentDateTimeUtc().addSecs(object.value(QStringLiteral("expires_in")).toInt(3600));
        exchangeXbox(accessToken, object.value(QStringLiteral("refresh_token")).toString(), expires);
    });
}

void MicrosoftAuth::refresh(const QString& refreshToken)
{
    if (m_clientId.isEmpty()) {
        emit failed(tr("no Microsoft app id is configured"));
        return;
    }
    if (refreshToken.isEmpty()) {
        emit failed(tr("there is no refresh token to use"));
        return;
    }

    const QByteArray body = form({ { QStringLiteral("grant_type"), QStringLiteral("refresh_token") },
                                   { QStringLiteral("client_id"), m_clientId },
                                   { QStringLiteral("refresh_token"), refreshToken },
                                   { QStringLiteral("scope"), QString::fromLatin1(kScope) } });
    postForm(m_endpoints.token, body, [this, refreshToken](bool ok, int, const QJsonObject& object, const QString& error) {
        if (!ok) {
            emit failed(tr("the sign-in could not be renewed: %1").arg(errorText(object, error)));
            return;
        }

        const QString accessToken = object.value(QStringLiteral("access_token")).toString();
        if (accessToken.isEmpty()) {
            emit failed(tr("Microsoft returned no access token"));
            return;
        }
        const QString renewed = object.value(QStringLiteral("refresh_token")).toString();
        const QDateTime expires = QDateTime::currentDateTimeUtc().addSecs(object.value(QStringLiteral("expires_in")).toInt(3600));
        exchangeXbox(accessToken, renewed.isEmpty() ? refreshToken : renewed, expires);
    });
}

void MicrosoftAuth::exchangeXbox(const QString& microsoftToken, const QString& refreshToken, const QDateTime& expires)
{
    const QJsonObject body{
        { QStringLiteral("Properties"),
          QJsonObject{ { QStringLiteral("AuthMethod"), QStringLiteral("RPS") },
                       { QStringLiteral("SiteName"), QStringLiteral("user.auth.xboxlive.com") },
                       { QStringLiteral("RpsTicket"), QStringLiteral("d=") + microsoftToken } } },
        { QStringLiteral("RelyingParty"), QStringLiteral("http://auth.xboxlive.com") },
        { QStringLiteral("TokenType"), QStringLiteral("JWT") },
    };
    postJson(m_endpoints.xboxUser, body, [this, refreshToken, expires](bool ok, int, const QJsonObject& object, const QString& error) {
        if (!ok) {
            emit failed(tr("Xbox Live refused the sign-in: %1").arg(errorText(object, error)));
            return;
        }
        const QString xboxToken = object.value(QStringLiteral("Token")).toString();
        if (xboxToken.isEmpty()) {
            emit failed(tr("Xbox Live returned no token"));
            return;
        }
        requestXsts(xboxToken, refreshToken, expires);
    });
}

void MicrosoftAuth::requestXsts(const QString& xboxToken, const QString& refreshToken, const QDateTime& expires)
{
    const QJsonObject body{
        { QStringLiteral("Properties"),
          QJsonObject{ { QStringLiteral("SandboxId"), QStringLiteral("RETAIL") },
                       { QStringLiteral("UserTokens"), QJsonArray{ xboxToken } } } },
        { QStringLiteral("RelyingParty"), QStringLiteral("rp://api.minecraftservices.com/") },
        { QStringLiteral("TokenType"), QStringLiteral("JWT") },
    };
    postJson(m_endpoints.xsts, body, [this, refreshToken, expires](bool ok, int, const QJsonObject& object, const QString& error) {
        if (!ok) {
            const QString code = object.value(QStringLiteral("XErr")).toString();
            if (code == QLatin1String("2148916233"))
                emit failed(tr("this Microsoft account has no Xbox profile yet; sign in to Xbox once"));
            else if (code == QLatin1String("2148916238"))
                emit failed(tr("this account is a child account and needs a family group"));
            else
                emit failed(tr("XSTS refused the sign-in: %1").arg(errorText(object, error)));
            return;
        }

        const QString token = object.value(QStringLiteral("Token")).toString();
        const QJsonArray claims = object.value(QStringLiteral("DisplayClaims")).toObject().value(QStringLiteral("xui")).toArray();
        const QJsonObject first = claims.isEmpty() ? QJsonObject() : claims.first().toObject();
        const QString userHash = first.value(QStringLiteral("uhs")).toString();
        const QString xuid = first.value(QStringLiteral("xid")).toString();
        if (token.isEmpty() || userHash.isEmpty()) {
            emit failed(tr("XSTS returned no token"));
            return;
        }
        m_session.xuid = xuid;
        requestMinecraftToken(token, userHash, refreshToken, expires);
    });
}

void MicrosoftAuth::requestMinecraftToken(const QString& xstsToken, const QString& userHash, const QString& refreshToken,
                                          const QDateTime& expires)
{
    const QJsonObject body{
        { QStringLiteral("identityToken"), QStringLiteral("XBL3.0 x=%1;%2").arg(userHash, xstsToken) },
    };
    postJson(m_endpoints.minecraftLogin, body, [this, refreshToken, expires](bool ok, int status, const QJsonObject& object, const QString& error) {
        if (!ok) {
            if (status == 401 || status == 403)
                emit failed(tr("Minecraft services refused this account"));
            else
                emit failed(tr("Minecraft services refused the sign-in: %1").arg(errorText(object, error)));
            return;
        }
        const QString token = object.value(QStringLiteral("access_token")).toString();
        if (token.isEmpty()) {
            emit failed(tr("Minecraft services returned no access token"));
            return;
        }
        requestProfile(token, refreshToken, expires);
    });
}

void MicrosoftAuth::requestProfile(const QString& minecraftToken, const QString& refreshToken, const QDateTime& expires)
{
    getJson(m_endpoints.minecraftProfile, minecraftToken, [this, minecraftToken, refreshToken, expires](
                                                                  bool ok, int status, const QJsonObject& object, const QString& error) {
        if (!ok) {
            if (status == 404)
                emit failed(tr("this account does not own Minecraft"));
            else
                emit failed(tr("the Minecraft profile could not be read: %1").arg(errorText(object, error)));
            return;
        }

        const QString name = object.value(QStringLiteral("name")).toString();
        const QString uuid = object.value(QStringLiteral("id")).toString();
        if (name.isEmpty() || uuid.isEmpty()) {
            emit failed(tr("the Minecraft profile came back without a name"));
            return;
        }

        m_session.username = name;
        m_session.uuid = uuid;
        m_session.accessToken = minecraftToken;
        m_session.refreshToken = refreshToken;
        m_session.accessExpires = expires;
        emit signedIn();
    });
}

}  // namespace Komorebi::Services
