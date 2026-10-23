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

#include <QDialog>

#include "minecraft/auth/MinecraftAccount.h"

class QLineEdit;
class QLabel;
class QPushButton;

/** Asks for an authentication server and the login there, and hands back the account once the server accepts it. */
class YggdrasilLoginDialog final : public QDialog {
    Q_OBJECT

   public:
    explicit YggdrasilLoginDialog(QWidget* parent = nullptr);
    ~YggdrasilLoginDialog() override = default;

    auto account() const -> MinecraftAccountPtr { return m_account; }

    static auto newAccount(QWidget* parent) -> MinecraftAccountPtr;

    /** Why the typed address cannot be used, or empty when it can. */
    static auto serverProblem(const QString& text) -> QString;

   private slots:
    void signIn();
    void updateAcceptAllowed();

   private:
    auto makeAccount(const QString& profileChoice) -> MinecraftAccountPtr;
    bool runLogin(const MinecraftAccountPtr& account);

    QLineEdit* m_server;
    QLineEdit* m_login;
    QLineEdit* m_password;
    QLabel* m_hint;
    QPushButton* m_ok;
    MinecraftAccountPtr m_account;
    bool m_choiceRefused = false;
};
