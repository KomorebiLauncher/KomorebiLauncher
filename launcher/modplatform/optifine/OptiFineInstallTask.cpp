// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "OptiFineInstallTask.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QTimer>

#include "Application.h"
#include "FileSystem.h"
#include "java/JavaInstall.h"
#include "java/JavaInstallList.h"
#include "java/JavaUtils.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "net/Request.h"
#include "settings/SettingsObject.h"

namespace OptiFine {

InstallTask::InstallTask(MinecraftInstance* instance, Build build, QString mirrorBase)
    : m_instance(instance), m_build(std::move(build)), m_mirrorBase(std::move(mirrorBase))
{}

InstallTask::~InstallTask()
{
    delete m_process;
}

bool InstallTask::abort()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
    if (m_current) {
        m_current->abort();
    }
    emitAborted();
    return true;
}

void InstallTask::failWith(const QString& message)
{
    m_current.reset();
    emitFailed(message);
}

void InstallTask::executeTask()
{
    m_scratch = std::make_unique<QTemporaryDir>();
    if (!m_scratch->isValid()) {
        emitFailed(tr("Could not create a temporary directory for the OptiFine installer."));
        return;
    }

    auto tempRoot = m_scratch->path();
#ifdef Q_OS_WIN
    // Older OptiFine installers take the .minecraft location from APPDATA instead of an argument.
    m_appDataDir = FS::PathCombine(tempRoot, "AppData", "Roaming");
    m_scratchRoot = FS::PathCombine(m_appDataDir, ".minecraft");
#else
    m_scratchRoot = FS::PathCombine(tempRoot, ".minecraft");
#endif
    QDir().mkpath(FS::PathCombine(m_scratchRoot, "versions"));
    QDir().mkpath(FS::PathCombine(m_scratchRoot, "libraries"));

    /*
     * The installer ends by registering the new profile in the launcher's own profile file. Without one it puts up
     * a dialog of its own behind our back, so it gets a throwaway file it is allowed to edit.
     */
    auto profilesPath = FS::PathCombine(m_scratchRoot, QStringLiteral("launcher_profiles.json"));
    const QByteArray profilesJson = QByteArrayLiteral("{\"profiles\":{}}");
    QFile profiles(profilesPath);
    if (!QFileInfo::exists(profilesPath)
        && (!profiles.open(QIODevice::WriteOnly) || profiles.write(profilesJson) != profilesJson.size())) {
        failWith(tr("Could not create the profile file the OptiFine installer needs."));
        return;
    }

    fetchManifest();
}

void InstallTask::fetch(QUrl url, std::function<void(const QByteArray&)> ok, const QString& what, int attemptsLeft)
{
    auto [request, body] = Net::Request::makeByteArray(url);
    request->setNetwork(APPLICATION->network());
    m_current = request;
    connect(request.get(), &Task::failed, this, [this, url, ok, what, attemptsLeft](const QString& reason) {
        if (retryLater([this, url, ok, what, attemptsLeft] { fetch(url, ok, what, attemptsLeft - 1); }, what, attemptsLeft, reason)) {
            return;
        }
        failWith(tr("Could not download %1: %2").arg(what, reason));
    });
    connect(request.get(), &Task::succeeded, this, [body, ok = std::move(ok)]() { ok(*body); });
    request->start();
}

void InstallTask::fetchToFile(QUrl url, QString path, std::function<void()> ok, const QString& what, int attemptsLeft)
{
    FS::ensureFolderPathExists(QFileInfo(path).absolutePath());
    auto request = Net::Request::makeFile(url, path);
    request->setNetwork(APPLICATION->network());
    m_current = request;
    connect(request.get(), &Task::failed, this, [this, url, path, ok, what, attemptsLeft](const QString& reason) {
        if (retryLater([this, url, path, ok, what, attemptsLeft] { fetchToFile(url, path, ok, what, attemptsLeft - 1); }, what, attemptsLeft,
                       reason)) {
            return;
        }
        failWith(tr("Could not download %1: %2").arg(what, reason));
    });
    connect(request.get(), &Task::succeeded, this, [ok = std::move(ok)]() { ok(); });
    request->start();
}

auto InstallTask::retryLater(std::function<void()> again, const QString& what, int attemptsLeft, const QString& reason) -> bool
{
    if (attemptsLeft <= 0) {
        return false;
    }
    setStatus(tr("Trying again to download %1: %2").arg(what, reason));
    QTimer::singleShot(retryDelayMs, this, [this, again = std::move(again)] {
        // The player may have aborted during the wait, and an aborted task must not start another download.
        if (isRunning()) {
            again();
        }
    });
    return true;
}

void InstallTask::fetchManifest()
{
    setStatus(tr("Looking up Minecraft %1", "Finding the version OptiFine patches").arg(m_build.mcVersion));
    fetch(QUrl(QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json")),
          [this](const QByteArray& json) {
              QString versionUrl;
              if (auto doc = QJsonDocument::fromJson(json); doc.isObject()) {
                  for (const auto& entry : doc.object().value("versions").toArray()) {
                      auto obj = entry.toObject();
                      if (obj.value("id").toString() == m_build.mcVersion) {
                          versionUrl = obj.value("url").toString();
                          break;
                      }
                  }
              }
              if (versionUrl.isEmpty()) {
                  failWith(tr("Minecraft %1 was not found in the version manifest.").arg(m_build.mcVersion));
                  return;
              }
              fetchVersionJson(QUrl(versionUrl));
          },
          tr("the version manifest"));
}

void InstallTask::fetchVersionJson(QUrl url)
{
    setStatus(tr("Downloading the Minecraft %1 metadata", "The version OptiFine patches").arg(m_build.mcVersion));
    auto path = FS::PathCombine(m_scratchRoot, "versions", m_build.mcVersion, m_build.mcVersion + ".json");
    FS::ensureFolderPathExists(QFileInfo(path).absolutePath());
    fetch(url,
          [this, path](const QByteArray& json) {
              QSaveFile out(path);
              if (!out.open(QIODevice::WriteOnly) || out.write(json) != json.size() || !out.commit()) {
                  failWith(tr("Could not write the version metadata the installer needs."));
                  return;
              }
              QString jarUrl;
              if (auto doc = QJsonDocument::fromJson(json); doc.isObject()) {
                  jarUrl = doc.object().value("downloads").toObject().value("client").toObject().value("url").toString();
              }
              if (jarUrl.isEmpty()) {
                  failWith(tr("The version metadata has no client download."));
                  return;
              }
              fetchClientJar(QUrl(jarUrl));
          },
          tr("the version metadata"));
}

void InstallTask::fetchClientJar(QUrl url)
{
    setStatus(tr("Downloading the Minecraft %1 client", "The version OptiFine patches").arg(m_build.mcVersion));
    auto dir = FS::PathCombine(m_scratchRoot, "versions", m_build.mcVersion);
    fetchToFile(url, FS::PathCombine(dir, m_build.mcVersion + ".jar"), [this] { fetchOptiFineJar(); }, tr("the client jar"));
}

void InstallTask::fetchOptiFineJar()
{
    setStatus(tr("Downloading OptiFine %1", "The build being installed").arg(m_build.patch));
    auto filename = m_build.filename.isEmpty() ? QStringLiteral("OptiFine_%1_%2_%3.jar")
                                                      .arg(m_build.mcVersion, m_build.type, m_build.patch)
                                                : m_build.filename;
    fetchToFile(downloadUrl(m_mirrorBase, m_build), FS::PathCombine(m_scratchRoot, filename), [this, filename] {
        m_optiFineJar = FS::PathCombine(m_scratchRoot, filename);
        resolveJava();
    }, tr("OptiFine"));
}

/*
 * The installer is a jar, so it needs a JVM of its own. An explicit path wins because that is what
 * the instance will launch with anyway; otherwise the launcher's own detection is used, which is
 * what would otherwise only run as part of launching.
 */
void InstallTask::resolveJava()
{
    auto configured = m_instance->settings()->get("JavaPath").toString();
    if (QFileInfo::exists(configured)) {
        m_javaPath = configured;
        runInstaller();
        return;
    }

    setStatus(tr("Looking for a Java runtime to run the installer with"));
    auto javas = APPLICATION->javalist();
    m_current = javas->getLoadTask();
    connect(m_current.get(), &Task::finished, this, [this, javas] {
        m_current.reset();

        // Any runtime from 8 up can run an OptiFine installer, so take the newest one found.
        int newest = 0;
        QString found;
        for (int i = 0; i < javas->count(); i++) {
            auto java = std::dynamic_pointer_cast<JavaInstall>(javas->at(i));
            if (!java || java->id.major() < 8 || !QFileInfo::exists(java->path)) {
                continue;
            }
            if (java->id.major() > newest) {
                newest = java->id.major();
                found = java->path;
            }
        }
        if (found.isEmpty()) {
            JavaUtils utils;
            for (const auto& path : utils.FindJavaPaths()) {
                if (QFileInfo::exists(path)) {
                    found = path;
                    break;
                }
            }
        }
        if (found.isEmpty()) {
            failWith(tr("OptiFine needs a Java runtime to install. Launch this instance once so the launcher downloads one, or set a "
                        "Java path in its settings."));
            return;
        }

        m_javaPath = found;
        runInstaller();
    });
    m_current->start();
}

void InstallTask::runInstaller()
{
    setStatus(tr("Running the OptiFine installer"));

    auto runner = APPLICATION->getJarPath("OptiFineRunner.jar");
    if (!QFileInfo::exists(runner)) {
        failWith(tr("The OptiFine runner jar is missing from this build."));
        return;
    }

    delete m_process;
    m_process = new QProcess(this);
    auto env = QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_WIN
    env.insert("APPDATA", m_appDataDir);
#else
    env.insert("HOME", m_scratchRoot);
#endif
    m_process->setProcessEnvironment(env);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    /*
     * The installer registers the new profile in whichever launcher profile file it was given, which here is a
     * throwaway in the scratch tree. Success is still judged on the version json it wrote rather than on the exit
     * code, because the installer has more than one way to end badly after writing that file.
     */
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this] { harvest(); });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            failWith(tr("Could not start Java to run the OptiFine installer."));
            return;
        }
        harvest();
    });

    auto classPath = QStringList{ m_optiFineJar, runner }.join(QDir::listSeparator());
    m_process->start(m_javaPath, { QStringLiteral("-cp"), classPath, QStringLiteral("wtf.optifine.Runner"), m_scratchRoot });
}

void InstallTask::harvest()
{
    delete m_process;
    m_process = nullptr;

    auto id = versionId(m_build);
    auto producedPath = FS::PathCombine(m_scratchRoot, "versions", id, id + ".json");
    if (!QFileInfo::exists(producedPath)) {
        // The classic installer names its own profile; take whatever it wrote for this OptiFine.
        auto prefix = m_build.mcVersion + QStringLiteral("-OptiFine");
        QDirIterator it(FS::PathCombine(m_scratchRoot, "versions"), { QStringLiteral("*.json") }, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            auto candidate = it.next();
            if (QFileInfo(candidate).fileName().startsWith(prefix)) {
                producedPath = candidate;
                break;
            }
        }
    }
    if (!QFileInfo::exists(producedPath)) {
        failWith(tr("The OptiFine installer did not produce %1.").arg(id + ".json"));
        return;
    }

    QFile produced(producedPath);
    if (!produced.open(QIODevice::ReadOnly)) {
        failWith(tr("Could not read the version json produced by the installer."));
        return;
    }
    auto doc = QJsonDocument::fromJson(produced.readAll());
    produced.close();
    if (!doc.isObject()) {
        failWith(tr("The installer produced a version json that could not be parsed."));
        return;
    }
    /*
     * OptiFine names libraries that exist nowhere public - it forks launchwrapper and publishes neither - so
     * everything the installer left in its scratch tree moves into the instance and is read from there.
     */
    auto scratchLibraries = FS::PathCombine(m_scratchRoot, "libraries");
    auto instanceLibraries = FS::PathCombine(m_instance->instanceRoot(), "libraries");
    auto scratchRoot = QDir(scratchLibraries);

    QList<LocalLibrary> files;
    QDirIterator it(scratchLibraries, { QStringLiteral("*.jar") }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        auto source = it.next();
        auto storage = scratchRoot.relativeFilePath(source);
        auto target = FS::PathCombine(instanceLibraries, storage);
        auto name = QFileInfo(target).fileName();
        FS::ensureFolderPathExists(QFileInfo(target).absolutePath());
        if (QFileInfo::exists(target) && !QFile::remove(target)) {
            failWith(tr("Could not replace the %1 already in the instance.").arg(name));
            return;
        }
        if (!QFile::copy(source, target)) {
            failWith(tr("Could not copy %1 into the instance.").arg(name));
            return;
        }
        QFile copied(target);
        if (!copied.open(QIODevice::ReadOnly)) {
            failWith(tr("Could not read back the copied %1.").arg(name));
            return;
        }
        auto digest = QString::fromLatin1(QCryptographicHash::hash(copied.readAll(), QCryptographicHash::Sha1).toHex());
        files.append(LocalLibrary{ storage, QUrl::fromLocalFile(target).toString(), digest, QFileInfo(target).size() });
    }
    if (files.isEmpty()) {
        failWith(tr("The OptiFine installer did not extract anything."));
        return;
    }

    auto profile = buildComponent(doc.object(), m_build, files);

    auto staging = FS::PathCombine(m_scratchRoot, "optifine-component.json");
    QSaveFile out(staging);
    if (!out.open(QIODevice::WriteOnly) || out.write(QJsonDocument(profile).toJson(QJsonDocument::Indented)) < 0 || !out.commit()) {
        failWith(tr("Could not write the OptiFine component file."));
        return;
    }

    auto packProfile = m_instance->getPackProfile();
    // Reinstalling replaces the previous build; the component system refuses a second patch with
    // the same uid.
    if (packProfile->getComponent(componentUid) && !packProfile->remove(componentUid)) {
        failWith(tr("Could not replace the OptiFine component already in this instance."));
        return;
    }
    if (!packProfile->installComponents({ staging })) {
        failWith(tr("Could not add the OptiFine component to the instance."));
        return;
    }

    m_current.reset();
    emitSucceeded();
}

}  // namespace OptiFine
