// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "AuthlibInjectorDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

auto isUsableServer(const QString& text) -> bool
{
    const QUrl url(text.trimmed(), QUrl::TolerantMode);
    const auto scheme = url.scheme().toLower();
    return url.isValid() && !url.host().isEmpty() && (scheme == QLatin1String("https") || scheme == QLatin1String("http"));
}

}  // namespace

AuthlibInjectorDialog::AuthlibInjectorDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Install authlib-injector"));
    setWindowModality(Qt::WindowModal);

    auto layout = new QVBoxLayout(this);

    auto explanation = new QLabel(
        tr("The authentication server this instance should use. Once installed, the game sends its "
           "login, session and skin requests to that server instead of to Mojang."),
        this);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    m_url = new QLineEdit(this);
    m_url->setPlaceholderText(QStringLiteral("https://auth.example.com"));
    layout->addWidget(m_url);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Install"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_url, &QLineEdit::textChanged, this, [ok = buttons->button(QDialogButtonBox::Ok)](const QString& text) {
        ok->setEnabled(isUsableServer(text));
    });
    connect(m_url, &QLineEdit::returnPressed, buttons->button(QDialogButtonBox::Ok), &QPushButton::click);
    layout->addWidget(buttons);

    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    resize(460, 140);
}

auto AuthlibInjectorDialog::serverUrl() const -> QString
{
    return m_url->text().trimmed();
}
