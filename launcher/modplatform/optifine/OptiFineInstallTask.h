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
    void fetchClientJar(QUrl url, const QString& sha1);
    void fetchOptiFineJar();
    void resolveJava();
    void runInstaller();
    void harvest();

    /** Downloads into memory; the body is only valid while the callback runs. */
    void fetch(QUrl url, std::function<void(const QByteArray&)> ok, const QString& what, int attemptsLeft = downloadRetries);
    /** Downloads straight to disk, optionally verifying the sha1 the manifest published. */
    void fetchToFile(QUrl url,
                     QString path,
                     std::function<void()> ok,
                     const QString& what,
                     int attemptsLeft = downloadRetries,
                     const QString& sha1 = QString());

    /** Waits a moment and asks for the same file again, which is what a mirror node that hung up needs. */
    auto retryLater(std::function<void()> again, const QString& what, int attemptsLeft, const QString& reason) -> bool;

    void failWith(const QString& message);

    /** The mirror hands these files off to volunteer nodes, and those drop connections often enough to matter. */
    static constexpr int downloadRetries = 3;
    static constexpr int retryDelayMs = 1000;

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
    /** The installer's output is harvested exactly once, whichever way the process ends. */
    bool m_harvested = false;
    /** Set when the player cancelled: a killed installer must not install anything afterwards. */
    bool m_aborted = false;
};

}  // namespace OptiFine
