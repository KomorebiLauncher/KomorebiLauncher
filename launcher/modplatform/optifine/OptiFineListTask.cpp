// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Komorebi Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "OptiFineListTask.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "Application.h"
#include "net/Request.h"

namespace OptiFine {

ListTask::ListTask(QString mirrorBase, QString mcVersion)
    : m_mirrorBase(std::move(mirrorBase)), m_mcVersion(std::move(mcVersion))
{}

bool ListTask::abort()
{
    if (m_current) {
        m_current->abort();
    }
    emitAborted();
    return true;
}

void ListTask::executeTask()
{
    setStatus(tr("Loading the OptiFine build list"));

    auto base = m_mirrorBase.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    if (base.isEmpty()) {
        // The build list only exists on a mirror. An empty address means "no mirror" for every
        // other download, so report that rather than quietly using an address the player removed.
        emitFailed(tr("OptiFine's builds are only published on the download mirror, but no mirror address is set. "
                      "Add one in Settings > APIs, or restore the default."));
        return;
    }
    auto [request, body] = Net::Request::makeByteArray(QUrl(base + QStringLiteral("/optifine/versionList")));
    request->setNetwork(APPLICATION->network());
    m_current = request;

    connect(request.get(), &Task::failed, this, [this](const QString& reason) {
        m_current.reset();
        emitFailed(tr("Could not load the OptiFine build list: %1").arg(reason));
    });
    connect(request.get(), &Task::succeeded, this, [this, body]() {
        m_builds = buildsFor(parseBuilds(*body), m_mcVersion);
        m_current.reset();
        if (m_builds.isEmpty()) {
            emitFailed(tr("The mirror has no OptiFine build for Minecraft %1.").arg(m_mcVersion));
            return;
        }
        emitSucceeded();
    });
    request->start();
}

}  // namespace OptiFine
