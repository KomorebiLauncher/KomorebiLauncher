#include "ServerPingTask.h"

#include <QDebug>
#include <QElapsedTimer>
#include <memory>
#include <utility>

#include "McClient.h"
#include "McResolver.h"

ServerPingTask::ServerPingTask(QString domain, int port) : Task(), m_domain(std::move(domain)), m_port(port)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(pingTimeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(tr("The server did not answer within %1 seconds").arg(pingTimeoutMs / 1000)); });
}

void ServerPingTask::executeTask()
{
    qDebug() << "Querying status of" << QString("%1:%2").arg(m_domain).arg(m_port);

    // Task::start() may run a finished task again; the outcome flags belong to this attempt.
    m_finished = false;
    m_timeout.start();

    // Both are children, so dropping this task takes the open socket and the pending lookup with it.
    m_resolver = new McResolver(this, m_domain, m_port);
    connect(m_resolver, &McResolver::succeeded, this, [this](QString ip, int port) {
        if (m_finished) {
            return;
        }

        qDebug().nospace().noquote() << "Resolved address for " << m_domain << ": " << ip << ":" << port;

        m_client = new McClient(this, m_domain, ip, static_cast<uint16_t>(port));
        auto started = std::make_shared<QElapsedTimer>();
        started->start();

        connect(m_client, &McClient::succeeded, this, [this, started](QJsonObject data) {
            if (m_finished) {
                return;
            }
            m_outputStatus = readServerStatus(data);
            m_outputStatus.latencyMs = static_cast<int>(started->elapsed());
            succeed();
        });
        connect(m_client, &McClient::failed, this, [this](QString error) { fail(error); });
        connect(m_client, &McClient::finished, this, [this] {
            if (m_client) {
                m_client->deleteLater();
                m_client = nullptr;
            }
        });
        m_client->getStatusData();
    });
    connect(m_resolver, &McResolver::failed, this, [this](QString error) { fail(error); });
    connect(m_resolver, &McResolver::finished, this, [this] {
        if (m_resolver) {
            m_resolver->deleteLater();
            m_resolver = nullptr;
        }
    });
    m_resolver->ping();
}

void ServerPingTask::stopPeers()
{
    if (m_client) {
        m_client->deleteLater();
        m_client = nullptr;
    }
    if (m_resolver) {
        m_resolver->deleteLater();
        m_resolver = nullptr;
    }
}

void ServerPingTask::fail(const QString& error)
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    m_timeout.stop();
    stopPeers();
    m_outputStatus = {};
    emitFailed(error);
}

void ServerPingTask::succeed()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    m_timeout.stop();
    stopPeers();
    emitSucceeded();
}

bool ServerPingTask::abort()
{
    if (m_finished) {
        return true;
    }
    m_finished = true;
    m_timeout.stop();
    stopPeers();
    if (isRunning()) {
        emitAborted();
    }
    return true;
}
