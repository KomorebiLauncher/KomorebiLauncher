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

#include <algorithm>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
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
    return !text.trimmed().isEmpty() && YggdrasilLoginDialog::serverProblem(text).isEmpty();
}

auto askForCharacter(QWidget* parent, const QList<YggdrasilProfile>& profiles) -> QString
{
    QStringList names;
    for (const auto& profile : profiles) {
        // Two characters named the same can only be told apart by their id.
        auto shared = std::ranges::count(profiles, profile.name, &YggdrasilProfile::name) > 1;
        names.append(shared ? QStringLiteral("%1 (%2)").arg(profile.name, profile.id) : profile.name);
    }

    bool chose = false;
    auto name = QInputDialog::getItem(parent, QObject::tr("Choose a character"),
                                      QObject::tr("This account may play as more than one character. Which one should the launcher use?"),
                                      names, 0, false, &chose);
    if (!chose || name.isEmpty()) {
        return QString();
    }
    return profiles.at(names.indexOf(name)).id;
}

}  // namespace

YggdrasilLoginDialog::YggdrasilLoginDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Add a third-party account"));
    setWindowModality(Qt::WindowModal);

    auto layout = new QVBoxLayout(this);

    auto explanation = new QLabel(
        tr("Sign in to a server that answers the Yggdrasil API with a username and password. Type the authentication "
           "address the server gives its players, and the sign-in path goes below it. Instances that join an "
           "online-mode server on that account also need the authlib-injector agent, which this launcher installs on "
           "request."),
        this);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto form = new QFormLayout(this);
    m_server = new QLineEdit(this);
    m_server->setPlaceholderText(QStringLiteral("https://auth.example.com/yggdrasil"));
    form->addRow(tr("Authentication server"), m_server);

    m_login = new QLineEdit(this);
    form->addRow(tr("Username"), m_login);

    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Password"), m_password);
    layout->addLayout(form);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_hint->hide();
    layout->addWidget(m_hint);

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

auto YggdrasilLoginDialog::serverProblem(const QString& text) -> QString
{
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    // A URL is parsed tolerantly, which means a pasted address with its surrounding sentence still attached turns
    // into encoded path characters and a server that answers 404. Say so before the login is attempted.
    for (const auto& character : trimmed) {
        if (character.unicode() > 127 || character.isSpace()) {
            return tr("The address has to be only the web address the server gives, with no spaces or other text.");
        }
    }

    const QUrl url(trimmed, QUrl::TolerantMode);
    const auto scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty() || (scheme != QLatin1String("https") && scheme != QLatin1String("http"))) {
        return tr("The address has to start with http:// or https://");
    }
    return QString();
}

void YggdrasilLoginDialog::updateAcceptAllowed()
{
    auto problem = serverProblem(m_server->text());
    m_hint->setText(problem);
    m_hint->setVisible(!problem.isEmpty());

    m_ok->setEnabled(isUsableServer(m_server->text()) && !m_login->text().trimmed().isEmpty() && !m_password->text().isEmpty());
}

void YggdrasilLoginDialog::signIn()
{
    auto account = makeAccount(QString());
    if (!runLogin(account)) {
        return;
    }

    auto& state = *account->accountData();
    if (!m_choiceRefused && state.availableProfiles.size() > 1) {
        auto choice = askForCharacter(this, state.availableProfiles);
        if (!choice.isEmpty()) {
            auto chosen = makeAccount(choice);
            if (runLogin(chosen)) {
                m_account = chosen;
                accept();
                return;
            }
            // The server refused to be told which character to hand over. Its own pick still plays, and asking
            // again the next time the sign-in button is pressed would only refuse again.
            m_choiceRefused = true;
            return;
        }
    }

    m_account = account;
    accept();
}

auto YggdrasilLoginDialog::makeAccount(const QString& profileChoice) -> MinecraftAccountPtr
{
    auto account = MinecraftAccount::createYggdrasil(m_server->text(), m_login->text().trimmed(), m_password->text());
    account->accountData()->profileChoice = profileChoice;
    return account;
}

bool YggdrasilLoginDialog::runLogin(const MinecraftAccountPtr& account)
{
    auto flow = account->login();

    QString failure;
    connect(flow.get(), &Task::failed, this, [&failure](const QString& reason) { failure = reason; });

    ProgressDialog progress(this);
    if (progress.execWithTask(flow.get()) == QDialog::Accepted) {
        return true;
    }

    if (!failure.isEmpty()) {
        CustomMessageBox::selectable(this, tr("Add a third-party account"), failure, QMessageBox::Warning)->show();
    }
    return false;
}

auto YggdrasilLoginDialog::newAccount(QWidget* parent) -> MinecraftAccountPtr
{
    YggdrasilLoginDialog dialog(parent);
    if (dialog.exec() != QDialog::Accepted) {
        return nullptr;
    }
    return dialog.account();
}
