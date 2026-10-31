// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include "modplatform/optifine/OptiFineList.h"
#include "tasks/Task.h"

namespace OptiFine {

/** Downloads the mirror's OptiFine build list for one Minecraft version. */
class ListTask : public Task {
    Q_OBJECT
   public:
    ListTask(QString mirrorBase, QString mcVersion);

    bool canAbort() const override { return true; }
    bool abort() override;

    auto builds() const -> const QList<Build>& { return m_builds; }

   protected:
    void executeTask() override;

   private:
    QString m_mirrorBase;
    QString m_mcVersion;
    QList<Build> m_builds;
    Task::Ptr m_current;
};

}  // namespace OptiFine
