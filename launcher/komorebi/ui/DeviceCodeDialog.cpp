// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/ui/DeviceCodeDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "Application.h"
#include "komorebi/services/auth/MicrosoftAuth.h"

using namespace Komorebi::Services;

KomorebiDeviceCodeDialog::KomorebiDeviceCodeDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Sign in with Microsoft"));
    setMinimumWidth(460);

    m_network = APPLICATION->network();
    m_auth = new MicrosoftAuth(m_network, this);
    m_auth->setClientId(APPLICATION->getMSAClientID());

    auto* layout = new QVBoxLayout(this);

    m_instruction = new QLabel(this);
    m_instruction->setWordWrap(true);
    m_instruction->setText(tr("Open the address below in a browser and enter the code. This window "
                              "will wait for you."));
    layout->addWidget(m_instruction);

    m_codeLabel = new QLabel(this);
    m_codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_codeLabel->setAlignment(Qt::AlignCenter);
    QFont codeFont = m_codeLabel->font();
    codeFont.setPointSize(codeFont.pointSize() + 12);
    codeFont.setBold(true);
    codeFont.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    m_codeLabel->setFont(codeFont);
    m_codeLabel->setStyleSheet(QStringLiteral("background: rgba(250, 247, 240, 0.9); border: 1px solid #c9c2b4;"
                                              " border-radius: 6px; padding: 12px;"));
    layout->addWidget(m_codeLabel);

    auto* actions = new QHBoxLayout();
    m_copy = new QPushButton(tr("Copy code"), this);
    m_open = new QPushButton(tr("Open page"), this);
    m_open->setEnabled(false);
    actions->addWidget(m_copy);
    actions->addWidget(m_open);
    actions->addStretch(1);
    layout->addLayout(actions);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* buttons = new QDialogButtonBox(this);
    m_cancel = buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(m_copy, &QPushButton::clicked, this, &KomorebiDeviceCodeDialog::copyCode);
    connect(m_open, &QPushButton::clicked, this, [this] {
        if (!m_verificationUri.isEmpty())
            QDesktopServices::openUrl(QUrl(m_verificationUri));
    });
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_auth, &MicrosoftAuth::deviceCodeReady, this, &KomorebiDeviceCodeDialog::codeArrived);
    connect(m_auth, &MicrosoftAuth::signedIn, this, &KomorebiDeviceCodeDialog::signedIn);
    connect(m_auth, &MicrosoftAuth::failed, this, &KomorebiDeviceCodeDialog::failed);

    m_poll = new QTimer(this);
    connect(m_poll, &QTimer::timeout, this, &KomorebiDeviceCodeDialog::poll);

    m_copy->setEnabled(false);
    startSignIn();
}

KomorebiDeviceCodeDialog::~KomorebiDeviceCodeDialog() = default;

void KomorebiDeviceCodeDialog::startSignIn()
{
    m_status->setText(tr("Asking Microsoft for a code…"));
    m_auth->requestDeviceCode();
}

void KomorebiDeviceCodeDialog::codeArrived()
{
    m_code = m_auth->currentDeviceCode();
    m_verificationUri = m_code.verificationUri;

    m_codeLabel->setText(m_code.userCode);
    m_open->setEnabled(!m_verificationUri.isEmpty());
    m_copy->setEnabled(true);
    m_status->setText(tr("Waiting for you to approve the sign-in in the browser. The code is good for "
                         "%1 minutes.")
                          .arg(qMax(1, m_code.expiresIn / 60)));
    m_poll->start(m_code.interval * 1000);
}

void KomorebiDeviceCodeDialog::poll()
{
    m_auth->pollDeviceCode(m_code);
}

void KomorebiDeviceCodeDialog::signedIn()
{
    m_poll->stop();
    m_session = m_auth->session();
    accept();
}

void KomorebiDeviceCodeDialog::failed(const QString& reason)
{
    m_poll->stop();
    m_status->setText(tr("The sign-in could not finish: %1").arg(reason));
    m_copy->setEnabled(!m_code.userCode.isEmpty());
    m_cancel->setText(tr("Close"));
}

void KomorebiDeviceCodeDialog::copyCode()
{
    if (m_code.userCode.isEmpty())
        return;
    QApplication::clipboard()->setText(m_code.userCode);
    m_status->setText(tr("Code copied. Paste it into the browser."));
}
