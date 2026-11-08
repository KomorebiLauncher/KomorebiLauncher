// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Several files at once, and one answer when they are done.
 *
 *  A launch waits on all of them, so the queue counts: it starts a few, replaces each as it finishes,
 *  and reports once at the end whether anything failed. Failures are collected rather than raised --
 *  one missing library should not hide the three that never got tried.
 */

#pragma once

#include <QList>
#include <QObject>
#include <QStringList>

#include "komorebi/services/download/DownloadSource.h"
#include "komorebi/services/download/FileDownloader.h"

class QNetworkAccessManager;

namespace Komorebi::Services {

class DownloadQueue : public QObject {
    Q_OBJECT

   public:
    explicit DownloadQueue(QNetworkAccessManager* network, QObject* parent = nullptr);

    /// Fetches every file; finished() follows once each one has been tried.
    void start(const QList<FileDownload>& files, DownloadSource source, int parallel = 4);

    void cancel();

    bool isRunning() const { return m_running; }

   signals:
    void progress(int done, int total);
    void finished(bool ok, const QStringList& failures);

   private:
    void startNext();
    void fileFinished(FileDownloader* downloader, bool ok, const QString& error);
    void finishIfDone();

    QNetworkAccessManager* m_network = nullptr;
    QList<FileDownload> m_files;
    QList<FileDownloader*> m_active;
    QStringList m_failures;
    DownloadSource m_source = DownloadSource::Official;
    int m_next = 0;
    int m_parallel = 4;
    bool m_running = false;
};

}  // namespace Komorebi::Services
