// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>

/// A socket that serves a fixed set of files over HTTP, for tests that must not use a network.
class KomorebiTestServer : public QTcpServer {
   public:
    QHash<QByteArray, QByteArray> files;  ///< request path to body; anything else answers 404
    bool supportRange = false;
    int requests = 0;
    QByteArray lastPath;
    QByteArray lastRange;
    int dropFirstRequests = 0;

    using QTcpServer::QTcpServer;

   protected:
    void incomingConnection(qintptr handle) override
    {
        auto* socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);

        auto buffer = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer] {
            buffer->append(socket->readAll());
            const int headerEnd = buffer->indexOf("\r\n\r\n");
            if (headerEnd < 0)
                return;

            const QByteArray head = buffer->left(headerEnd);
            buffer->clear();
            ++requests;

            lastPath = head.split('\n').first().split(' ').value(1);
            lastRange.clear();
            for (const auto& line : head.split('\n')) {
                const QByteArray trimmed = line.trimmed();
                if (trimmed.toLower().startsWith("range:"))
                    lastRange = trimmed.mid(6).trimmed();
            }

            if (dropFirstRequests > 0) {
                --dropFirstRequests;
                socket->abort();
                return;
            }

            if (!files.contains(lastPath)) {
                socket->write("HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                socket->disconnectFromHost();
                return;
            }

            const QByteArray body = files.value(lastPath);
            qint64 start = 0;
            if (supportRange && lastRange.startsWith("bytes="))
                start = lastRange.mid(6).split('-').first().toLongLong();

            QByteArray responseHead;
            if (start > 0) {
                responseHead = "HTTP/1.1 206 Partial Content\r\nConnection: close\r\nContent-Length: "
                               + QByteArray::number(body.size() - start) + "\r\nContent-Range: bytes " + QByteArray::number(start)
                               + '-' + QByteArray::number(body.size() - 1) + '/' + QByteArray::number(body.size()) + "\r\n\r\n";
            } else {
                responseHead = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size())
                               + "\r\n\r\n";
            }
            socket->write(responseHead);
            socket->write(body.mid(start));
            socket->flush();
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
};
