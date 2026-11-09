// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Making a new instance from the launcher window.
 *
 *  The dialog asks for the two things a player actually chooses -- what to call it and which game version
 *  it runs -- and, when one is wanted, a loader. The work behind it is the core's: the dialog collects
 *  the answer, shows the steps, and keeps the window open until the instance exists or the reason it does
 *  not is readable.
 */

#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkReply;
class QProgressBar;
class QPushButton;

namespace Komorebi::Services {
class InstanceCreator;
}

class KomorebiNewInstanceDialog : public QDialog {
    Q_OBJECT

   public:
    explicit KomorebiNewInstanceDialog(QWidget* parent = nullptr);
    ~KomorebiNewInstanceDialog() override;

    /// Where the instance was written; empty until the dialog is accepted.
    QString createdInstanceDirectory() const { return m_createdDirectory; }

   private:
    void fetchVersions();
    void requestVersions();
    void versionsArrived(QNetworkReply* reply);
    void loaderChanged();
    void startCreation();
    void creationStatus(const QString& text);
    void creationProgress(int done, int total);
    void creationFinished(const QString& directory);
    void creationFailed(const QString& reason);

    QLineEdit* m_name = nullptr;
    QComboBox* m_version = nullptr;
    QComboBox* m_loader = nullptr;
    QLineEdit* m_loaderVersion = nullptr;
    QLabel* m_loaderVersionLabel = nullptr;
    QLabel* m_status = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_create = nullptr;
    QPushButton* m_cancel = nullptr;
    Komorebi::Services::InstanceCreator* m_creator = nullptr;
    QString m_createdDirectory;
    int m_versionAttempt = 0;
};
