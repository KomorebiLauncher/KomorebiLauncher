// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "YggdrasilLoginDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "tasks/Task.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"

#include "Application.h"
#include "minecraft/auth/AuthFlow.h"

namespace {

auto isUsableServer(const QString& text) -> bool
{
    const QUrl url(text.trimmed(), QUrl::TolerantMode);
    const auto scheme = url.scheme().toLower();
    return url.isValid() && !url.host().isEmpty() && (scheme == QLatin1String("https") || scheme == QLatin1String("http"));
}

}  // namespace

YggdrasilLoginDialog::YggdrasilLoginDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Add a third-party account"));
    setWindowModality(Qt::WindowModal);

    auto layout = new QVBoxLayout(this);

    auto explanation = new QLabel(
        tr("Sign in to an authentication server that runs authlib-injector. Instances that join an online-mode server on "
           "that account also need the authlib-injector agent, which this launcher installs on request."),
        this);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto form = new QFormLayout(this);
    m_server = new QLineEdit(this);
    m_server->setPlaceholderText(QStringLiteral("https://auth.example.com"));
    form->addRow(tr("Authentication server"), m_server);

    m_login = new QLineEdit(this);
    form->addRow(tr("Username"), m_login);

    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Password"), m_password);
    layout->addLayout(form);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setText(tr("Sign in"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &YggdrasilLoginDialog::signIn);
    layout->addWidget(buttons);

    for (auto field : { m_server, m_login, m_password }) {
        connect(field, &QLineEdit::textChanged, this, [this] { updateAcceptAllowed(); });
    }
    connect(m_password, &QLineEdit::returnPressed, m_ok, &QPushButton::click);

    updateAcceptAllowed();
    resize(480, 220);
}

void YggdrasilLoginDialog::updateAcceptAllowed()
{
    m_ok->setEnabled(isUsableServer(m_server->text()) && !m_login->text().trimmed().isEmpty() && !m_password->text().isEmpty());
}

void YggdrasilLoginDialog::signIn()
{
    auto account = MinecraftAccount::createYggdrasil(m_server->text(), m_login->text().trimmed(), m_password->text());
    auto flow = account->login();

    QString failure;
    connect(flow.get(), &Task::failed, this, [&failure](const QString& reason) { failure = reason; });

    ProgressDialog progress(this);
    if (progress.execWithTask(flow.get()) == QDialog::Accepted) {
        m_account = account;
        accept();
        return;
    }

    if (!failure.isEmpty()) {
        CustomMessageBox::selectable(this, tr("Add a third-party account"), failure, QMessageBox::Warning)->show();
    }
}

auto YggdrasilLoginDialog::newAccount(QWidget* parent) -> MinecraftAccountPtr
{
    YggdrasilLoginDialog dialog(parent);
    if (dialog.exec() != QDialog::Accepted) {
        return nullptr;
    }
    return dialog.account();
}
