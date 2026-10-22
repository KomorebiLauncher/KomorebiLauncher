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

auto endpoint(const QString& serverUrl, const QString& operation) -> QUrl
{
    auto base = serverUrl.trimmed();
    while (base.endsWith('/')) {
        base.chop(1);
    }
    if (!base.endsWith(QLatin1String("/api/yggdrasil"))) {
        base += QLatin1String("/api/yggdrasil");
    }
    return QUrl(base + QLatin1Char('/') + operation, QUrl::TolerantMode);
}

auto clientTokenOf(const AccountData* data) -> QString
{
    auto stored = data->yggdrasilToken.extra.value(QStringLiteral("clientToken")).toString();
    return stored.isEmpty() ? QUuid::createUuid().toString(QUuid::Id128) : stored;
}

}  // namespace

YggdrasilStep::YggdrasilStep(AccountData* data, bool interactive) : AuthStep(data), m_interactive(interactive) {}

QString YggdrasilStep::describe()
{
    return m_interactive ? tr("Logging in to the authentication server") : tr("Renewing the access token");
}

void YggdrasilStep::perform()
{
    auto url = endpoint(m_data->authServerUrl, m_interactive ? QStringLiteral("authenticate") : QStringLiteral("refresh"));

    QJsonObject body;
    if (m_interactive) {
        body[QStringLiteral("agent")] = QJsonObject{ { QStringLiteral("name"), QStringLiteral("Minecraft") },
                                                     { QStringLiteral("version"), QStringLiteral("1.5") } };
        body[QStringLiteral("username")] = m_data->loginName;
        body[QStringLiteral("password")] = m_data->password;
        body[QStringLiteral("requestUser")] = true;
    } else {
        body[QStringLiteral("accessToken")] = m_data->yggdrasilToken.token;
    }
    body[QStringLiteral("clientToken")] = clientTokenOf(m_data);

    auto requestBody = QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact));

    auto headers = QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
    };

    auto [request, response] = Net::Request::makeByteArray(url, requestBody.toUtf8());
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));

    m_task.reset(new NetJob("YggdrasilStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
    qDebug() << "Sending" << url.toString() << "request";
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
        if (status == 401 || status == 403) {
            emit finished(AccountTaskState::STATE_FAILED_HARD, reason);
        } else if (status >= 400 && status < 500) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT, reason);
        } else {
            m_data->networkError = m_request->error();
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Failed to contact the authentication server: %1").arg(reason));
        }
        return;
    }

    if (!Parsers::parseYggdrasilResponse(*response, m_data->yggdrasilToken, m_data->minecraftProfile)) {
        qWarning() << "Could not parse the authentication server response...";
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("The authentication server sent an answer that could not be understood."));
        return;
    }

    if (m_data->minecraftProfile.id.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("The authentication server gave no character to play as."));
        return;
    }

    // The server decides who owns the game, and it just said yes by handing out a token.
    m_data->minecraftEntitlement.ownsMinecraft = true;
    m_data->minecraftEntitlement.canPlayMinecraft = true;
    m_data->minecraftEntitlement.validity = Validity::Certain;

    emit finished(AccountTaskState::STATE_WORKING, tr("Got an access token"));
}
