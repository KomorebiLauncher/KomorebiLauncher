// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  One file, fetched to a known path and checked before it is trusted.
 *
 *  A launcher's downloads fail in three ordinary ways: the connection drops, the player closes the
 *  launcher halfway, or the file arrives damaged. The first is retried, the second resumes from what
 *  is already on disk, and the third is what the checksum is for. None of them should leave a file
 *  the launcher will later believe is complete, so bytes go to `<name>.part` until they are checked.
 */

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

namespace Komorebi::Services {

/// What the launcher knows about a file before fetching it, from the manifest that names it.
struct FileDownload {
    QUrl url;
    QString targetPath;
    QByteArray sha1;          ///< empty when the manifest gives no hash
    qint64 expectedSize = -1;  ///< negative when the manifest gives no size
};

class FileDownloader : public QObject {
    Q_OBJECT

   public:
    explicit FileDownloader(QNetworkAccessManager* network, QObject* parent = nullptr);
    ~FileDownloader() override;

    /// Starts the download; finished() follows, once.
    void start(const FileDownload& download);

    void cancel();

   signals:
    void progress(qint64 received, qint64 total);
    void finished(bool ok, const QString& error);

   private:
    void beginAttempt();
    void handleReadyRead();
    void handleReplyFinished();
    void fail(const QString& error);
    bool finishFile();
    bool existingFileIsGood() const;
    static QByteArray fileHash(const QString& path);

    QNetworkAccessManager* m_network = nullptr;
    FileDownload m_download;
    QNetworkReply* m_reply = nullptr;
    QFile* m_file = nullptr;
    QString m_partPath;
    qint64 m_received = 0;
    qint64 m_total = -1;
    int m_attempt = 0;
    bool m_cancelled = false;
    bool m_reported = false;
};

}  // namespace Komorebi::Services
