// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/download/DownloadQueue.h"

#include <QNetworkAccessManager>

namespace Komorebi::Services {

DownloadQueue::DownloadQueue(QNetworkAccessManager* network, QObject* parent) : QObject(parent), m_network(network)
{
}

void DownloadQueue::start(const QList<FileDownload>& files, DownloadSource source, int parallel)
{
    m_files = files;
    m_source = source;
    m_parallel = parallel > 0 ? parallel : 1;
    m_next = 0;
    m_active.clear();
    m_failures.clear();
    m_running = true;

    if (m_files.isEmpty()) {
        m_running = false;
        QMetaObject::invokeMethod(
            this,
            [this] {
                emit progress(0, 0);
                emit finished(true, QStringList());
            },
            Qt::QueuedConnection);
        return;
    }

    startNext();
}

void DownloadQueue::cancel()
{
    if (!m_running)
        return;
    m_running = false;

    const auto active = m_active;
    m_active.clear();
    for (auto* downloader : active) {
        downloader->disconnect(this);
        downloader->cancel();
        downloader->deleteLater();
    }

    QStringList failures = m_failures;
    failures.append(tr("cancelled"));
    emit finished(false, failures);
}

void DownloadQueue::startNext()
{
    while (m_running && m_active.size() < m_parallel && m_next < m_files.size()) {
        FileDownload request = m_files.at(m_next);
        ++m_next;

        // The manifest keeps the official URL; the source decides where it is actually fetched from,
        // and stays the fallback when the mirror turns out not to have the file.
        const QUrl official = request.url;
        request.url = sourceUrl(official, m_source);
        if (request.url != official)
            request.fallbackUrl = official;

        auto* downloader = new FileDownloader(m_network, this);
        m_active.append(downloader);
        connect(downloader, &FileDownloader::finished, this,
                [this, downloader](bool ok, const QString& error) { fileFinished(downloader, ok, error); });
        downloader->start(request);
    }
    finishIfDone();
}

void DownloadQueue::fileFinished(FileDownloader* downloader, bool ok, const QString& error)
{
    if (!m_running)
        return;

    m_active.removeAll(downloader);
    downloader->deleteLater();

    if (!ok)
        m_failures.append(error);

    emit progress(m_next - m_active.size(), m_files.size());
    startNext();
}

void DownloadQueue::finishIfDone()
{
    if (!m_running || m_next < m_files.size() || !m_active.isEmpty())
        return;

    m_running = false;
    emit progress(m_files.size(), m_files.size());
    emit finished(m_failures.isEmpty(), m_failures);
}

}  // namespace Komorebi::Services
