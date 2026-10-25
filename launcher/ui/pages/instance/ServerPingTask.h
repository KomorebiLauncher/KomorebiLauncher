#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "ServerStatus.h"
#include "tasks/Task.h"

class McClient;
class McResolver;

/**
 * One server-list status ping: SRV/A resolution, the protocol exchange, and a deadline.
 *
 * A server that accepts the connection and then answers nothing used to leave its row showing
 * "..." for good, and because the ping could not be aborted, pressing refresh could not replace it
 * either. Both are bounded here: the deadline fails the task, and abort() drops the socket and the
 * lookup instead of waiting for them.
 */
class ServerPingTask : public Task {
    Q_OBJECT
   public:
    explicit ServerPingTask(QString domain, int port);
    ServerStatus m_outputStatus;

    bool canAbort() const override { return true; }
    bool abort() override;

   protected:
    void executeTask() override;

   private:
    void fail(const QString& error);
    void succeed();
    void stopPeers();

    /** Long enough for a slow SRV lookup plus a handshake, short enough not to look hung. */
    static constexpr int pingTimeoutMs = 15000;

    QString m_domain;
    int m_port;
    QTimer m_timeout;
    McClient* m_client = nullptr;
    McResolver* m_resolver = nullptr;
    bool m_finished = false;
};
