#include <QElapsedTimer>
#include <QFutureWatcher>

#include <Json.h>
#include "McClient.h"
#include "McResolver.h"
#include "Result.h"
#include "ServerPingTask.h"

void ServerPingTask::executeTask()
{
    qDebug() << "Querying status of" << QString("%1:%2").arg(m_domain).arg(m_port);

    // Resolve the actual IP and port for the server
    McResolver* resolver = new McResolver(nullptr, m_domain, m_port);
    connect(resolver, &McResolver::succeeded, this, [this](QString ip, int port) {
        qDebug().nospace().noquote() << "Resolved address for " << m_domain << ": " << ip << ":" << port;

        // Now that we have the IP and port, query the server
        McClient* client = new McClient(nullptr, m_domain, ip, port);
        auto started = std::make_shared<QElapsedTimer>();
        started->start();

        connect(client, &McClient::succeeded, this, [this, started](QJsonObject data) {
            m_outputStatus = readServerStatus(data);
            m_outputStatus.latencyMs = static_cast<int>(started->elapsed());
            qDebug() << "Server status:" << m_outputStatus.online << "of" << m_outputStatus.max << "in" << m_outputStatus.latencyMs << "ms";
            emitSucceeded();
        });
        connect(client, &McClient::failed, this, [this](QString error) { emitFailed(error); });

        // Delete McClient object when done
        connect(client, &McClient::finished, this, [client]() { client->deleteLater(); });
        client->getStatusData();
    });
    connect(resolver, &McResolver::failed, this, [this](QString error) { emitFailed(error); });

    // Delete McResolver object when done
    connect(resolver, &McResolver::finished, resolver, &McResolver::deleteLater);
    resolver->ping();
}
