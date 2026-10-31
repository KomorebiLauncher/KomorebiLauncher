// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once
#include <QJsonObject>
#include <QObject>
#include <QUrl>

#include "minecraft/auth/AuthStep.h"
#include "net/NetJob.h"
#include "net/Request.h"

/** Trades a login for an access token, or renews the token, at a server that answers the Yggdrasil API. */
class YggdrasilStep : public AuthStep {
    Q_OBJECT

   public:
    explicit YggdrasilStep(AccountData* data, bool interactive);
    virtual ~YggdrasilStep() noexcept = default;

    void perform() override;

    QString describe() override;

    /** The address of one of the server's operations, below the authserver the server address is the root of. */
    static auto endpoint(const QString& serverUrl, const QString& operation) -> QUrl;

    /** What gets sent: a login for the token, or the token to have it renewed. */
    static auto requestBody(const AccountData& data, bool interactive) -> QJsonObject;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    bool m_interactive;
    QUrl m_url;
    Net::Request::Ptr m_request;
    NetJob::Ptr m_task;
};
