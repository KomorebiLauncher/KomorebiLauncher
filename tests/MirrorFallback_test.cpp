// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QEventLoop>
#include <QHash>
#include <QNetworkAccessManager>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include "net/MirrorPolicy.h"
#include "net/Request.h"

/** Minimal HTTP/1.1 server: registered paths answer 200 with their body, everything else 404s. */
class TinyServer : public QObject {
    Q_OBJECT
   public:
    explicit TinyServer(QObject* parent = nullptr) : QObject(parent) { m_server.setParent(this); }

    void respond(const QString& path, const QByteArray& body) { m_bodies.insert(path, body); }

    bool listen()
    {
        if (!m_server.listen(QHostAddress::LocalHost, 0))
            return false;
        connect(&m_server, &QTcpServer::newConnection, this, [this] { handle(m_server.nextPendingConnection()); });
        return true;
    }

    auto url(const QString& path) const { return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path)); }

   private:
    void handle(QTcpSocket* socket)
    {
        socket->setParent(this);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            const QByteArray request = socket->readAll();
            const auto fields = QString::fromUtf8(request).split(QLatin1Char(' '));
            const QString path = fields.size() > 1 ? fields.at(1) : QString();
            if (!m_bodies.contains(path)) {
                socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            } else {
                const QByteArray body = m_bodies.value(path);
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) +
                              "\r\nConnection: close\r\n\r\n" + body);
            }
            socket->flush();
            socket->disconnectFromHost();
        });
    }

    QTcpServer m_server;
    QHash<QString, QByteArray> m_bodies;
};

class MirrorFallbackTest : public QObject {
    Q_OBJECT
   private:
    QNetworkAccessManager m_nam;

    /** Runs the request to completion and returns how it ended. */
    auto run(Net::Request::Ptr request) -> QString
    {
        QEventLoop loop;
        QString outcome = QStringLiteral("never finished");
        request->setNetwork(&m_nam);
        connect(request.get(), &Task::succeeded, &loop, [&] {
            outcome = QStringLiteral("succeeded");
            loop.quit();
        });
        connect(request.get(), &Task::failed, &loop, [&](const QString& reason) {
            outcome = QStringLiteral("failed");
            loop.quit();
        });
        request->start();
        loop.exec();
        return outcome;
    }

   private slots:
    void test_FallsBackToTheNextSource()
    {
        TinyServer server;
        server.respond(QStringLiteral("/present"), QByteArrayLiteral("hello-from-the-second-source"));
        QVERIFY(server.listen());

        auto [request, bodyPtr] = Net::Request::makeByteArray(server.url(QStringLiteral("/missing")));
        request->setSources(Net::SourceSequence({ server.url(QStringLiteral("/missing")), server.url(QStringLiteral("/present")) }));

        auto outcome = run(request);
        QCOMPARE(outcome, QStringLiteral("succeeded"));
        // The bytes must come from the second source, not an empty result from the first.
        QCOMPARE(*bodyPtr, QByteArrayLiteral("hello-from-the-second-source"));
        QCOMPARE(request->url(), server.url(QStringLiteral("/present")));
    }

    void test_StopsWhenNoSourceIsLeft()
    {
        TinyServer server;
        server.respond(QStringLiteral("/present"), QByteArrayLiteral("unused"));
        QVERIFY(server.listen());

        auto [request, bodyPtr] = Net::Request::makeByteArray(server.url(QStringLiteral("/missing")));
        request->setSources(Net::SourceSequence({ server.url(QStringLiteral("/missing")) }));

        // Exactly one source: the request must fail rather than loop.
        QCOMPARE(run(request), QStringLiteral("failed"));
    }

    void test_FirstSourceWinningDoesNotRetry()
    {
        TinyServer server;
        server.respond(QStringLiteral("/present"), QByteArrayLiteral("first-source"));
        QVERIFY(server.listen());

        auto [request, bodyPtr] = Net::Request::makeByteArray(server.url(QStringLiteral("/present")));
        auto sources = request->sources();  // no mirror applies to a loopback host

        QCOMPARE(run(request), QStringLiteral("succeeded"));
        QCOMPARE(sources.urls().size(), size_t(1));
        QCOMPARE(*bodyPtr, QByteArrayLiteral("first-source"));
    }

    /** Instance-local OptiFine jars are referenced with file:// URLs, so the downloader has to accept them. */
    void test_FileUrlIsFetchable()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto path = QDir(dir.path()).filePath("local-library.jar");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("local-library-bytes");
        file.close();

        auto [request, bodyPtr] = Net::Request::makeByteArray(QUrl::fromLocalFile(path));
        QCOMPARE(run(request), QStringLiteral("succeeded"));
        QCOMPARE(*bodyPtr, QByteArrayLiteral("local-library-bytes"));
    }
};

QTEST_GUILESS_MAIN(MirrorFallbackTest)

#include "MirrorFallback_test.moc"
