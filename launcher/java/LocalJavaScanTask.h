// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Komorebi Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "JavaInstall.h"
#include "java/JavaChecker.h"
#include "tasks/Task.h"

namespace Java {

/**
 * Verifies the Java runtimes that live inside an instance folder.
 *
 * The version is only known by running the binary, so a directory name such as "jre-8" proves
 * nothing; every candidate is checked with JavaChecker before it is offered.
 */
class LocalJavaScanTask : public Task {
    Q_OBJECT
   public:
    using Ptr = shared_qobject_ptr<class LocalJavaScanTask>;

    explicit LocalJavaScanTask(QString instanceRoot, QObject* parent = nullptr);

    /** Verified installations, ordered by descending version so the newest match wins. */
    auto installs() const -> const QList<JavaInstallPtr>& { return m_installs; }

    /**
     * Java executables under <root>/runtime and <root>/.minecraft/runtime, searched a few
     * directory levels deep: the launcher's own runtimes sit at <root>/<name>/bin, while the
     * official launcher unpacks them at <root>/<component>/<platform>/<component>/bin.
     */
    static auto findCandidates(const QString& instanceRoot) -> QStringList;

    bool canAbort() const override { return true; }
    bool abort() override;

   protected:
    void executeTask() override;

   private:
    void scanFinished();

    QString m_root;
    QList<JavaInstallPtr> m_installs;
    QList<JavaChecker::Result> m_results;
    Task::Ptr m_job;
};

}  // namespace Java
