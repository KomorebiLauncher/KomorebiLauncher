// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "YggdrasilStep.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUuid>
#include <QUrl>

#include "Application.h"
#include "Logging.h"
#include "minecraft/auth/Parsers.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"
#include "net/Request.h"

namespace {

/** The client token the server bound this account to, or empty when it never gave one. */
auto storedClientTokenOf(const AccountData* data) -> QString
{
    return data->yggdrasilToken.extra.value(QStringLiteral("clientToken")).toString();
}

}  // namespace

YggdrasilStep::YggdrasilStep(AccountData* data, bool interactive) : AuthStep(data), m_interactive(interactive) {}

QString YggdrasilStep::describe()
{
    return m_interactive ? tr("Logging in to the authentication server") : tr("Renewing the access token");
}

auto YggdrasilStep::endpoint(const QString& serverUrl, const QString& operation) -> QUrl
{
    /*
     * The address a server hands its players is the root of both APIs it hosts, because the session lookups live
     * under sessionserver next to authserver. So the operation goes below authserver, and nothing else is invented.
     *
     * Parsed rather than concatenated: an address pasted with a query or a fragment on the end would otherwise
     * swallow the operation, sending the sign-in to a path that does not exist.
     */
    QUrl base(serverUrl.trimmed(), QUrl::TolerantMode);
    base.setQuery(QString());
    base.setFragment(QString());

    auto path = base.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    base.setPath(path + QLatin1String("/authserver/") + operation);
    return base;
}

auto YggdrasilStep::requestBody(const AccountData& data, bool interactive) -> QJsonObject
{
    QJsonObject body;
    if (interactive) {
        QJsonObject agent{ { QStringLiteral("name"), QStringLiteral("Minecraft") }, { QStringLiteral("version"), 1 } };
        // Servers that let one account hold several characters pick one themselves unless it is told which.
        if (!data.profileChoice.isEmpty()) {
            agent[QStringLiteral("id")] = data.profileChoice;
        }
        body[QStringLiteral("agent")] = agent;
        body[QStringLiteral("username")] = data.loginName;
        body[QStringLiteral("password")] = data.password;
        body[QStringLiteral("requestUser")] = true;
    } else {
        body[QStringLiteral("accessToken")] = data.yggdrasilToken.token;
    }
    /*
     * A refresh has to present the client token the sign-in was made with; inventing a new one is
     * exactly what makes a strict server refuse it. Only a fresh sign-in establishes one.
     */
    auto clientToken = storedClientTokenOf(&data);
    if (clientToken.isEmpty() && interactive) {
        clientToken = QUuid::createUuid().toString(QUuid::Id128);
    }
    if (!clientToken.isEmpty()) {
        body[QStringLiteral("clientToken")] = clientToken;
    }
    return body;
}

void YggdrasilStep::perform()
{
    m_url = endpoint(m_data->authServerUrl, m_interactive ? QStringLiteral("authenticate") : QStringLiteral("refresh"));

    auto body = requestBody(*m_data, m_interactive);
    auto requestBodyText = QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact));

    auto headers = QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
    };

    auto [request, response] = Net::Request::makeByteArray(m_url, requestBodyText.toUtf8());
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));

    m_task.reset(new NetJob("YggdrasilStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
    qDebug() << "Sending" << m_url.toString() << "request";
}

void YggdrasilStep::onRequestDone(QByteArray* response)
{
    if (m_request->error() != QNetworkReply::NoError) {
        // The reason a login got refused - wrong password, banned, no character yet - is only in the body of
        // the error answer, and it is the one thing the user can do something about.
        auto reason = Parsers::parseYggdrasilError(m_request->errorResponse());
        if (reason.isEmpty()) {
            reason = m_request->errorString();
        }
        auto status = m_request->replyStatusCode();
        qWarning() << "Yggdrasil request failed with" << status << ":" << reason;
        if (status == 404) {
            // 404 is not a refusal, it is the address having no such operation, which is what an address typed one
            // step too far looks like from here.
            emit finished(AccountTaskState::STATE_FAILED_SOFT,
                          tr("Nothing answers sign-in at %1. The address should be the one the server gives its "
                             "players, without the authserver part on the end.")
                              .arg(m_url.toString()));
        } else if (status == 401 || status == 403) {
            emit finished(AccountTaskState::STATE_FAILED_HARD, reason);
        } else if (status >= 400 && status < 500) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT, reason);
        } else {
            m_data->networkError = m_request->error();
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Failed to contact the authentication server: %1").arg(reason));
        }
        return;
    }

    if (!Parsers::parseYggdrasilResponse(*response, m_data->yggdrasilToken, m_data->minecraftProfile, m_data->availableProfiles)) {
        qWarning() << "Could not parse the authentication server response...";
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("The authentication server sent an answer that could not be understood."));
        return;
    }

    if (m_data->minecraftProfile.id.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("The authentication server gave no character to play as."));
        return;
    }

    if (!m_data->profileChoice.isEmpty() && m_data->minecraftProfile.id != m_data->profileChoice) {
        qWarning() << "The authentication server handed over" << m_data->minecraftProfile.name << "instead of the asked-for character";
    }

    // The server decides who owns the game, and it just said yes by handing out a token.
    m_data->minecraftEntitlement.ownsMinecraft = true;
    m_data->minecraftEntitlement.canPlayMinecraft = true;
    m_data->minecraftEntitlement.validity = Validity::Certain;

    emit finished(AccountTaskState::STATE_WORKING, tr("Got an access token"));
}
