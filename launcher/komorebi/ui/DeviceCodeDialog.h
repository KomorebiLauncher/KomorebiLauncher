// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Signing in to Microsoft without leaving the launcher: a code to type into a browser, and a
 *  window that waits for it. The polling runs on a timer, so the window stays usable while the
 *  player is in the browser -- the thing a modal loop would take away.
 */

#pragma once

#include <QDialog>

#include "komorebi/services/auth/AuthSession.h"

class QLabel;
class QNetworkAccessManager;
class QPushButton;
class QTimer;

namespace Komorebi::Services {
class MicrosoftAuth;
}

class KomorebiDeviceCodeDialog : public QDialog {
    Q_OBJECT

   public:
    explicit KomorebiDeviceCodeDialog(QWidget* parent = nullptr);
    ~KomorebiDeviceCodeDialog() override;

    /// The account, once the dialog has been accepted.
    Komorebi::Services::AuthSession session() const { return m_session; }

   private:
    void startSignIn();
    void codeArrived();
    void poll();
    void signedIn();
    void failed(const QString& reason);
    void copyCode();

    QNetworkAccessManager* m_network = nullptr;
    Komorebi::Services::MicrosoftAuth* m_auth = nullptr;
    QTimer* m_poll = nullptr;
    Komorebi::Services::DeviceCode m_code;
    Komorebi::Services::AuthSession m_session;
    QString m_verificationUri;

    QLabel* m_instruction = nullptr;
    QLabel* m_codeLabel = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_copy = nullptr;
    QPushButton* m_open = nullptr;
    QPushButton* m_cancel = nullptr;
};
