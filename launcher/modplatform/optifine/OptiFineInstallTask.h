// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QTemporaryDir>
#include <functional>
#include <memory>

#include "modplatform/optifine/OptiFineList.h"
#include "tasks/Task.h"

class MinecraftInstance;
class QProcess;

namespace OptiFine {

/**
 * Installs one OptiFine build into an instance.
 *
 * OptiFine's own installer is the only official patcher, and what it produces differs by
 * generation - some builds reference launchwrapper from maven, others ship a patched
 * launchwrapper-of inside the OptiFine jar - so the installer decides the shape and we harvest
 * its output rather than reproducing its rules.
 *
 * The harvested json becomes an instance component. OptiFine's own jar has no public maven home,
 * so it is copied into the instance's libraries folder and referenced by a file:// URL, which
 * keeps it durable and travelling with the instance.
 */
class InstallTask : public Task {
    Q_OBJECT
   public:
    InstallTask(MinecraftInstance* instance, Build build, QString mirrorBase);
    ~InstallTask() override;

    bool canAbort() const override { return true; }
    bool abort() override;


   protected:
    void executeTask() override;

   private:
    void fetchManifest();
    void fetchVersionJson(QUrl url);
    void fetchClientJar(QUrl url);
    void fetchOptiFineJar();
    void resolveJava();
    void runInstaller();
    void harvest();

    /** Downloads into memory; the body is only valid while the callback runs. */
    void fetch(QUrl url, std::function<void(const QByteArray&)> ok, const QString& what);
    /** Downloads straight to disk. */
    void fetchToFile(QUrl url, QString path, std::function<void()> ok, const QString& what);

    void failWith(const QString& message);

    MinecraftInstance* m_instance;
    Build m_build;
    QString m_mirrorBase;
    QString m_javaPath;
    QString m_scratchRoot;
    QString m_appDataDir;  ///< Windows only: where APPDATA points, so the installer finds .minecraft
    QString m_optiFineJar;
    std::unique_ptr<QTemporaryDir> m_scratch;

    Task::Ptr m_current;
    QProcess* m_process = nullptr;
};

}  // namespace OptiFine
