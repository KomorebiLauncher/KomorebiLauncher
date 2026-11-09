// SPDX-License-Identifier: GPL-3.0-only
#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include "komorebi/services/download/FileDownloader.h"

using namespace Komorebi::Services;

namespace {

/// A server that answers one file and then forgets the connection; that is all a fetch of one file needs.
class TinyFileServer : public QTcpServer {
   public:
    QByteArray body;
    bool supportRange = false;
    int requests = 0;
    int dropFirstRequests = 0;
    QByteArray lastRange;

    using QTcpServer::QTcpServer;

   protected:
    void incomingConnection(qintptr handle) override
    {
        auto* socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);

        auto buffer = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer] {
            buffer->append(socket->readAll());
            if (!buffer->contains("\r\n\r\n"))
                return;

            ++requests;
            lastRange.clear();
            for (const auto& line : buffer->split('\n')) {
                const QByteArray trimmed = line.trimmed();
                if (trimmed.toLower().startsWith("range:"))
                    lastRange = trimmed.mid(6).trimmed();
            }
            buffer->clear();

            if (dropFirstRequests > 0) {
                --dropFirstRequests;
                socket->abort();
                return;
            }

            qint64 start = 0;
            if (supportRange && lastRange.startsWith("bytes="))
                start = lastRange.mid(6).split('-').first().toLongLong();

            QByteArray head;
            if (start > 0) {
                head = "HTTP/1.1 206 Partial Content\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size() - start)
                       + "\r\nContent-Range: bytes " + QByteArray::number(start) + '-' + QByteArray::number(body.size() - 1) + '/'
                       + QByteArray::number(body.size()) + "\r\n\r\n";
            } else {
                head = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
            }
            socket->write(head);
            socket->write(body.mid(start));
            socket->flush();
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
};

QByteArray sha1Of(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha1);
}

QByteArray readAll(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

}  // namespace

class KomorebiFileDownloaderTest : public QObject {
    Q_OBJECT

   private slots:
    void downloadsAndChecksHash();
    void rejectsAWrongHash();
    void resumesFromAPartFile();
    void restartsWhenTheServerIgnoresRange();
    void skipsAFileThatIsAlreadyRight();
    void retriesAfterADroppedConnection();
    void writesAFileLargeEnoughForSeveralReads();

   private:
    static QUrl urlFor(const TinyFileServer& server);
};

QUrl KomorebiFileDownloaderTest::urlFor(const TinyFileServer& server)
{
    return QUrl(QStringLiteral("http://127.0.0.1:%1/file.bin").arg(server.serverPort()));
}

void KomorebiFileDownloaderTest::downloadsAndChecksHash()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    TinyFileServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.body = QByteArrayLiteral("a small file the launcher asked for");

    QNetworkAccessManager network;
    FileDownloader downloader(&network);
    QSignalSpy spy(&downloader, &FileDownloader::finished);
    const QString target = temp.filePath(QStringLiteral("file.bin"));
    downloader.start(FileDownload{ urlFor(server), target, sha1Of(server.body), server.body.size() });

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 10000);
    QVERIFY(spy.first().at(0).toBool());
    QCOMPARE(readAll(target), server.body);
    QVERIFY(!QFile::exists(target + QStringLiteral(".part")));
}

void KomorebiFileDownloaderTest::rejectsAWrongHash()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    TinyFileServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.body = QByteArrayLiteral("the bytes that arrived");

    QNetworkAccessManager network;
    FileDownloader downloader(&network);
    QSignalSpy spy(&downloader, &FileDownloader::finished);
    const QString target = temp.filePath(QStringLiteral("file.bin"));
    downloader.start(FileDownload{ urlFor(server), target, sha1Of(QByteArrayLiteral("the bytes the manifest named")), -1 });

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 10000);
    QVERIFY(!spy.first().at(0).toBool());
    QVERIFY(!QFile::exists(target));
    QVERIFY(!QFile::exists(target + QStringLiteral(".part")));
}

void KomorebiFileDownloaderTest::resumesFromAPartFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    TinyFileServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.supportRange = true;
    server.body = QByteArray(4096, 'x');

    const QString target = temp.filePath(QStringLiteral("file.bin"));
    {
        QFile part(target + QStringLiteral(".part"));
        QVERIFY(part.open(QIODevice::WriteOnly));
        part.write(server.body.left(1024));
    }

    QNetworkAccessManager network;
    FileDownloader downloader(&network);
    QSignalSpy spy(&downloader, &FileDownloader::finished);
    downloader.start(FileDownload{ urlFor(server), target, sha1Of(server.body), server.body.size() });

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 10000);
    QVERIFY2(spy.first().at(0).toBool(), qPrintable(spy.first().at(1).toString()));
    QCOMPARE(readAll(target), server.body);
    QCOMPARE(server.lastRange, QByteArrayLiteral("bytes=1024-"));
}

void KomorebiFileDownloaderTest::restartsWhenTheServerIgnoresRange()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    TinyFileServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.body = QByteArray(2048, 'y');

    const QString target = temp.filePath(QStringLiteral("file.bin"));
    {
        QFile part(target + QStringLiteral(".part"));
        QVERIFY(part.open(QIODevice::WriteOnly));
        part.write(server.body.left(512));
    }

    QNetworkAccessManager network;
    FileDownloader downloader(&network);
    QSignalSpy spy(&downloader, &FileDownloader::finished);
    downloader.start(FileDownload{ urlFor(server), target, sha1Of(server.body), server.body.size() });

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 10000);
    QVERIFY(spy.first().at(0).toBool());
    QCOMPARE(readAll(target), server.body);
}

void KomorebiFileDownloaderTest::skipsAFileThatIsAlreadyRight()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    TinyFileServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.body = QByteArrayLiteral("already here");

    const QString target = temp.filePath(QStringLiteral("file.bin"));
    {
        QFile file(target);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(server.body);
    }

    QNetworkAccessManager network;
    FileDownloader downloader(&network);
    QSignalSpy spy(&downloader, &FileDownloader::finished);
    downloader.start(FileDownload{ urlFor(server), target, sha1Of(server.body), server.body.size() });

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 10000);
    QVERIFY(spy.first().at(0).toBool());
    QCOMPARE(server.requests, 0);
}

void KomorebiFileDownloaderTest::writesAFileLargeEnoughForSeveralReads()
{
    // A download that arrives in several chunks used to be truncated at every chunk: the "server
    // ignored the range, start over" branch fired on every read because a normal answer is also 200.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    TinyFileServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.body = QByteArray(512 * 1024, 'z');

    QNetworkAccessManager network;
    FileDownloader downloader(&network);
    QSignalSpy spy(&downloader, &FileDownloader::finished);
    const QString target = temp.filePath(QStringLiteral("big.bin"));
    downloader.start(FileDownload{ urlFor(server), target, sha1Of(server.body), server.body.size() });

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 20000);
    QVERIFY2(spy.first().at(0).toBool(), qPrintable(spy.first().at(1).toString()));
    QCOMPARE(readAll(target).size(), server.body.size());
    QCOMPARE(readAll(target), server.body);
}
void KomorebiFileDownloaderTest::retriesAfterADroppedConnection()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    TinyFileServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.dropFirstRequests = 1;
    server.body = QByteArrayLiteral("the second attempt arrives");

    QNetworkAccessManager network;
    FileDownloader downloader(&network);
    QSignalSpy spy(&downloader, &FileDownloader::finished);
    const QString target = temp.filePath(QStringLiteral("file.bin"));
    downloader.start(FileDownload{ urlFor(server), target, sha1Of(server.body), -1 });

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 15000);
    QVERIFY2(spy.first().at(0).toBool(), qPrintable(spy.first().at(1).toString()));
    QCOMPARE(readAll(target), server.body);
    QVERIFY(server.requests >= 2);
}

QTEST_GUILESS_MAIN(KomorebiFileDownloaderTest)

#include "KomorebiFileDownloader_test.moc"
