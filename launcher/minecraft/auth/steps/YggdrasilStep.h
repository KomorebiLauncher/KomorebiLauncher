// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once
#include <QObject>

#include "minecraft/auth/AuthStep.h"
#include "net/NetJob.h"
#include "net/Request.h"

/** Trades a login for an access token, or renews the token, at an authlib-injector server. */
class YggdrasilStep : public AuthStep {
    Q_OBJECT

   public:
    explicit YggdrasilStep(AccountData* data, bool interactive);
    virtual ~YggdrasilStep() noexcept = default;

    void perform() override;

    QString describe() override;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    bool m_interactive;
    Net::Request::Ptr m_request;
    NetJob::Ptr m_task;
};
