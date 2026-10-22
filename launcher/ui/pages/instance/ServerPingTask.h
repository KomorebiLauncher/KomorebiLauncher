#pragma once

#include <QObject>
#include <QString>

#include "ServerStatus.h"
#include "tasks/Task.h"

class ServerPingTask : public Task {
    Q_OBJECT
   public:
    explicit ServerPingTask(QString domain, int port) : Task(), m_domain(domain), m_port(port) {}
    ~ServerPingTask() override = default;
    ServerStatus m_outputStatus;

   private:
    QString m_domain;
    int m_port;

   protected:
    virtual void executeTask() override;
};
