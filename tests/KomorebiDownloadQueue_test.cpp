// SPDX-License-Identifier: GPL-3.0-only
#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "KomorebiTestServer.h"
#include "komorebi/services/download/DownloadQueue.h"

using namespace Komorebi::Services;

class KomorebiDownloadQueueTest : public QObject {
    Q_OBJECT

   private slots:
    void fetchesEveryFile();
    void reportsTheOnesThatFailed();
    void fetchesNothingQuickly();

   private:
    static FileDownload requestFor(const KomorebiTestServer& server, const QString& path, const QString& target);
};

FileDownload KomorebiDownloadQueueTest::requestFor(const KomorebiTestServer& server, const QString& path, const QString& target)
{
    return FileDownload{ QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(server.serverPort()).arg(path)), target, {}, -1 };
}

void KomorebiDownloadQueueTest::fetchesEveryFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    KomorebiTestServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QList<FileDownload> files;
    for (int index = 0; index < 6; ++index) {
        const QString path = QStringLiteral("/file%1.bin").arg(index);
        server.files.insert(path.toLatin1(), QByteArrayLiteral("body of ") + QByteArray::number(index));
        files.append(requestFor(server, path, temp.filePath(QStringLiteral("file%1.bin").arg(index))));
    }

    QNetworkAccessManager network;
    DownloadQueue queue(&network);
    QSignalSpy spy(&queue, &DownloadQueue::finished);
    queue.start(files, DownloadSource::Official, 3);

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 15000);
    QVERIFY2(spy.first().at(0).toBool(), qPrintable(spy.first().at(1).toStringList().join(QStringLiteral("; "))));
    QCOMPARE(server.requests, 6);
    for (int index = 0; index < 6; ++index) {
        QFile file(temp.filePath(QStringLiteral("file%1.bin").arg(index)));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArrayLiteral("body of ") + QByteArray::number(index));
    }
}

void KomorebiDownloadQueueTest::reportsTheOnesThatFailed()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    KomorebiTestServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.files.insert("/present.bin", QByteArrayLiteral("here"));

    QList<FileDownload> files{ requestFor(server, QStringLiteral("/present.bin"), temp.filePath(QStringLiteral("present.bin"))),
                               requestFor(server, QStringLiteral("/absent.bin"), temp.filePath(QStringLiteral("absent.bin"))) };

    QNetworkAccessManager network;
    DownloadQueue queue(&network);
    QSignalSpy spy(&queue, &DownloadQueue::finished);
    queue.start(files, DownloadSource::Official, 2);

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 15000);
    QVERIFY(!spy.first().at(0).toBool());
    QCOMPARE(spy.first().at(1).toStringList().size(), 1);
    QVERIFY(QFile::exists(temp.filePath(QStringLiteral("present.bin"))));
    QVERIFY(!QFile::exists(temp.filePath(QStringLiteral("absent.bin"))));
}

void KomorebiDownloadQueueTest::fetchesNothingQuickly()
{
    QNetworkAccessManager network;
    DownloadQueue queue(&network);
    QSignalSpy spy(&queue, &DownloadQueue::finished);
    queue.start({}, DownloadSource::Official, 4);

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
    QVERIFY(spy.first().at(0).toBool());
}

QTEST_GUILESS_MAIN(KomorebiDownloadQueueTest)

#include "KomorebiDownloadQueue_test.moc"
