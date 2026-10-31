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

#include <QDialog>

class QLineEdit;

/** Asks for the authentication server authlib-injector should point the client at. */
class AuthlibInjectorDialog final : public QDialog {
    Q_OBJECT

   public:
    explicit AuthlibInjectorDialog(QWidget* parent = nullptr);

    auto serverUrl() const -> QString;

   private:
    QLineEdit* m_url;
};
