// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Komorebi Contributors
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

namespace {
/** Deep enough for the official launcher's <component>/<platform>/<component>/bin layout. */
constexpr int maxScanDepth = 4;

/**
 * Collects <dir>/bin/<javaExecutable> and descends into the subdirectories, up to maxScanDepth.
 *
 * The official launcher nests its runtimes four levels below the runtime root, so looking only at
 * the direct children finds nothing on a machine that has never used this launcher's own downloader.
 */
void scanForRuntimes(const QDir& dir, int depth, QStringList& found)
{
    auto appendIfPresent = [&found](const QDir& candidate) {
        auto executable = FS::PathCombine(candidate.absolutePath(), QStringLiteral("bin"), JavaUtils::javaExecutable);
        if (QFileInfo::exists(executable))
            found.append(QDir::cleanPath(executable));
    };

    appendIfPresent(dir);
    if (depth >= maxScanDepth)
        return;

    const auto children = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto& child : children) {
        scanForRuntimes(QDir(dir.absoluteFilePath(child)), depth + 1, found);
    }
}
}  // namespace

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

        scanForRuntimes(root, 0, found);
    }

    found.removeDuplicates();
    found.sort();
    return found;
}

void LocalJavaScanTask::executeTask()
{
    setStatus(tr("Looking for Java inside the instance"));

    // A restarted task must not report the runtimes of the previous run as well.
    m_results.clear();
    m_installs.clear();
    setAbortable(true);

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

    // Aborted while the checks were running: their results are not wanted.
    if (!isRunning()) {
        m_results.clear();
        return;
    }

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

    /*
     * Newest first: an instance folder may hold several runtimes and the highest usable one is the
     * better default. JavaInstall's own ordering leads with the architecture name, which would rank
     * a 32-bit runtime above a 64-bit one, so the keys are spelled out here.
     */
    std::sort(m_installs.begin(), m_installs.end(), [](const JavaInstallPtr& a, const JavaInstallPtr& b) {
        if (a->id == b->id) {
            if (a->is_64bit != b->is_64bit)
                return a->is_64bit;
            return a->path < b->path;
        }
        return b->id < a->id;
    });

    m_results.clear();
    emitSucceeded();
}

bool LocalJavaScanTask::abort()
{
    if (!isRunning()) {
        return true;
    }

    /*
     * This task is marked aborted before the job is cancelled: cancelling it can make it report an
     * outcome synchronously, and a cancelled scan must not also report success.
     */
    emitAborted();
    if (m_job) {
        m_job->abort();
    }
    return true;
}

}  // namespace Java
