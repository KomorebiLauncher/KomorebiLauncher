// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "LocalJavaScanTask.h"

#include <QDir>

#include "Application.h"
#include "FileSystem.h"
#include "settings/SettingsObject.h"
#include "java/JavaUtils.h"
#include "tasks/ConcurrentTask.h"

namespace Java {

LocalJavaScanTask::LocalJavaScanTask(QString instanceRoot, QObject* parent)
    : Task(parent), m_root(std::move(instanceRoot))
{}

auto LocalJavaScanTask::findCandidates(const QString& instanceRoot) -> QStringList
{
    QStringList found;
    const QStringList runtimeRoots = {
        FS::PathCombine(instanceRoot, QStringLiteral("runtime")),
        FS::PathCombine(instanceRoot, QStringLiteral(".minecraft"), QStringLiteral("runtime")),
    };

    for (const auto& runtimeRoot : runtimeRoots) {
        QDir root(runtimeRoot);
        if (!root.exists())
            continue;

        auto appendIfPresent = [&found](const QString& dir) {
            auto executable = FS::PathCombine(dir, QStringLiteral("bin"), JavaUtils::javaExecutable);
            if (QFileInfo::exists(executable))
                found.append(QDir::cleanPath(executable));
        };

        appendIfPresent(root.absolutePath());
        const auto children = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto& child : children) {
            appendIfPresent(root.absoluteFilePath(child));
        }
    }

    found.removeDuplicates();
    found.sort();
    return found;
}

void LocalJavaScanTask::executeTask()
{
    setStatus(tr("Looking for Java inside the instance"));

    auto candidates = findCandidates(m_root);
    if (candidates.isEmpty()) {
        emitSucceeded();
        return;
    }

    // APPLICATION_DYN: this task is also driven by unit tests, which have no Application.
    auto* app = APPLICATION_DYN;
    auto concurrency = app ? app->settings()->get("NumberOfConcurrentTasks").toInt() : 4;
    auto job = makeShared<ConcurrentTask>(tr("Java detection"), concurrency);
    m_job = job;
    connect(job.get(), &Task::finished, this, &LocalJavaScanTask::scanFinished);
    connect(job.get(), &Task::progress, this, &LocalJavaScanTask::setProgress);

    int id = 0;
    for (const auto& candidate : candidates) {
        auto* checker = new JavaChecker(candidate, QString(), 0, 0, 0, id++);
        connect(checker, &JavaChecker::checkFinished, this,
                [this](const JavaChecker::Result& result) { m_results.append(result); });
        job->addTask(Task::Ptr(checker));
    }

    m_job->start();
}

void LocalJavaScanTask::scanFinished()
{
    m_job.reset();
    std::sort(m_results.begin(), m_results.end(), [](const JavaChecker::Result& a, const JavaChecker::Result& b) { return a.id < b.id; });

    for (const auto& result : m_results) {
        if (result.validity != JavaChecker::Result::Validity::Valid)
            continue;

        auto install = std::make_shared<JavaInstall>();
        install->id = result.javaVersion;
        install->arch = result.realPlatform;
        install->path = result.path;
        install->is_64bit = result.is_64bit;
        m_installs.append(install);
    }

    // Newest first: an instance folder may hold several runtimes and the highest usable one is
    // the better default.
    std::sort(m_installs.begin(), m_installs.end(), [](const JavaInstallPtr& a, const JavaInstallPtr& b) { return *a > *b; });

    m_results.clear();
    emitSucceeded();
}

}  // namespace Java
