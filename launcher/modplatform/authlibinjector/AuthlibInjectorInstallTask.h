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

#include <functional>

#include "modplatform/authlibinjector/AuthlibInjector.h"
#include "tasks/Task.h"

class MinecraftInstance;

namespace AuthlibInjector {

/**
 * Puts authlib-injector into an instance as a component.
 *
 * The jar is copied into the instance's libraries folder and referenced by a file:// URL so it
 * travels with the instance, and the component declares it as a Java agent with the authentication
 * server as its argument. The artifact is pinned by the sha256 the index publishes, which is the
 * same checksum the upstream release carries.
 */
class InstallTask : public Task {
    Q_OBJECT
   public:
    InstallTask(MinecraftInstance* instance, QString serverUrl, QString mirrorBase);
    ~InstallTask() override;

    bool canAbort() const override { return true; }
    bool abort() override;

   protected:
    void executeTask() override;

   private:
    void fetchRelease();
    void fetchJar();
    void writeComponent();

    /** Downloads into memory; the body is only valid while the callback runs. */
    void fetch(QUrl url, std::function<void(const QByteArray&)> ok, const QString& what);
    /** Downloads straight to disk, verifying the sha256 the index gave us. */
    void fetchToFile(QUrl url, QString path, QString sha256, std::function<void()> ok, const QString& what);

    void failWith(const QString& message);

    MinecraftInstance* m_instance;
    QString m_serverUrl;
    QString m_mirrorBase;
    Release m_release;
    QString m_jarPath;
    Task::Ptr m_current;
};

}  // namespace AuthlibInjector
