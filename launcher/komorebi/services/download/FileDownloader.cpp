// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/download/FileDownloader.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace Komorebi::Services {

namespace {

constexpr int kMaxAttempts = 3;

QByteArray hashFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&file))
        return {};
    return hash.result();
}

}  // namespace

FileDownloader::FileDownloader(QNetworkAccessManager* network, QObject* parent) : QObject(parent), m_network(network)
{
}

FileDownloader::~FileDownloader()
{
    // A download left running would hold a reply, a file and a part file, and the reply's signals would
    // arrive at a half-destructed object. Drop them without telling anyone the download ended.
    m_cancelled = true;
    m_reported = true;
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    delete m_file;
    m_file = nullptr;
}

void FileDownloader::start(const FileDownload& download)
{
    m_download = download;
    m_cancelled = false;
    m_reported = false;
    m_received = 0;
    m_total = -1;
    m_attempt = 0;
    m_partPath = download.targetPath + QStringLiteral(".part");

    if (existingFileIsGood()) {
        m_reported = true;
        QMetaObject::invokeMethod(
            this,
            [this] {
                emit progress(m_download.expectedSize, m_download.expectedSize);
                emit finished(true, QString());
            },
            Qt::QueuedConnection);
        return;
    }

    QDir().mkpath(QFileInfo(download.targetPath).absolutePath());
    beginAttempt();
}

void FileDownloader::cancel()
{
    if (m_cancelled || m_reported)
        return;
    m_cancelled = true;
    m_reported = true;

    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    emit finished(false, QStringLiteral("cancelled"));
}

bool FileDownloader::existingFileIsGood() const
{
    const QFileInfo info(m_download.targetPath);
    if (!info.isFile())
        return false;

    if (!m_download.sha1.isEmpty())
        return hashFile(m_download.targetPath) == m_download.sha1;
    if (m_download.expectedSize >= 0)
        return info.size() == m_download.expectedSize;

    // Nothing to check it against: a file of size zero is the only one that cannot be a partial
    // download of something.
    return info.size() > 0;
}

void FileDownloader::beginAttempt()
{
    if (m_cancelled || m_reported)
        return;

    if (m_attempt >= kMaxAttempts) {
        fail(tr("the download did not succeed after %1 attempts").arg(kMaxAttempts));
        return;
    }
    ++m_attempt;

    const QFileInfo part(m_partPath);
    const qint64 alreadyThere = part.isFile() ? part.size() : 0;
    m_file = new QFile(m_partPath);
    const auto mode = alreadyThere > 0 ? (QIODevice::WriteOnly | QIODevice::Append) : (QIODevice::WriteOnly | QIODevice::Truncate);
    if (!m_file->open(mode)) {
        const QString reason = m_file->errorString();
        delete m_file;
        m_file = nullptr;
        fail(tr("could not open %1 for writing: %2").arg(m_partPath, reason));
        return;
    }
    m_received = alreadyThere;

    QNetworkRequest request(m_download.url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("KomorebiLauncher"));
    if (m_received > 0)
        request.setRawHeader("Range", QByteArrayLiteral("bytes=") + QByteArray::number(m_received) + QByteArrayLiteral("-"));

    m_reply = m_network->get(request);
    m_total = m_received > 0 ? m_received : -1;

    connect(m_reply, &QNetworkReply::readyRead, this, &FileDownloader::handleReadyRead);
    connect(m_reply, &QNetworkReply::errorOccurred, this, [this](QNetworkReply::NetworkError) { handleReplyFinished(); });
    connect(m_reply, &QNetworkReply::finished, this, &FileDownloader::handleReplyFinished);
}

void FileDownloader::handleReadyRead()
{
    if (!m_reply || !m_file || m_cancelled || m_reported)
        return;

    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 200 && m_received > 0) {
        // The server ignored the range and is sending the whole file; keeping the part would write it
        // twice, so start over.
        m_file->close();
        if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            const QString reason = m_file->errorString();
            fail(tr("could not rewrite %1: %2").arg(m_partPath, reason));
            return;
        }
        m_received = 0;
        m_total = -1;
    }

    const auto chunk = m_reply->readAll();
    if (chunk.isEmpty())
        return;

    if (m_file->write(chunk) != chunk.size()) {
        fail(tr("could not write to %1: %2").arg(m_partPath, m_file->errorString()));
        return;
    }
    m_received += chunk.size();

    if (m_total < 0) {
        const qint64 length = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        if (length > 0)
            m_total = length + (status == 206 ? m_received - chunk.size() : 0);
    }
    emit progress(m_received, m_total);
}

void FileDownloader::handleReplyFinished()
{
    if (!m_reply || m_cancelled || m_reported)
        return;

    auto* reply = m_reply;
    m_reply = nullptr;
    reply->disconnect(this);

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool httpOk = status == 0 || (status >= 200 && status < 300);
    const QString networkError = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
    reply->deleteLater();

    if (m_file) {
        m_file->flush();
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    if (!httpOk || !networkError.isEmpty()) {
        // A dropped connection is worth another attempt, and the part file is what it will resume from.
        if (m_attempt < kMaxAttempts && !networkError.contains(QStringLiteral("aborted"))) {
            QTimer::singleShot(300 * m_attempt, this, &FileDownloader::beginAttempt);
            return;
        }
        fail(networkError.isEmpty() ? tr("the server answered with HTTP %1").arg(status) : networkError);
        return;
    }

    finishFile();
}

bool FileDownloader::finishFile()
{
    const QFileInfo part(m_partPath);
    if (!part.isFile()) {
        fail(tr("the download wrote no file"));
        return false;
    }

    if (!m_download.sha1.isEmpty()) {
        const QByteArray actual = hashFile(m_partPath);
        if (actual != m_download.sha1) {
            QFile::remove(m_partPath);
            fail(tr("the file's SHA-1 is %1, not the %2 the manifest asked for")
                     .arg(QString::fromLatin1(actual.toHex()), QString::fromLatin1(m_download.sha1.toHex())));
            return false;
        }
    } else if (m_download.expectedSize >= 0 && part.size() != m_download.expectedSize) {
        QFile::remove(m_partPath);
        fail(tr("the file is %1 bytes, not the %2 the manifest asked for").arg(part.size()).arg(m_download.expectedSize));
        return false;
    }

    if (QFile::exists(m_download.targetPath) && !QFile::remove(m_download.targetPath)) {
        fail(tr("could not replace %1").arg(m_download.targetPath));
        return false;
    }
    if (!QFile::rename(m_partPath, m_download.targetPath)) {
        fail(tr("could not move %1 into place").arg(m_partPath));
        return false;
    }

    m_reported = true;
    emit progress(part.size(), part.size());
    emit finished(true, QString());
    return true;
}

void FileDownloader::fail(const QString& error)
{
    if (m_reported)
        return;
    m_reported = true;
    emit finished(false, error);
}

QByteArray FileDownloader::fileHash(const QString& path)
{
    return hashFile(path);
}

}  // namespace Komorebi::Services
