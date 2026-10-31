// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "AuthlibInjectorInstallTask.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>

#include "Application.h"
#include "FileSystem.h"
#include "minecraft/Library.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "net/ChecksumValidator.h"
#include "net/Request.h"
#include "settings/SettingsObject.h"

namespace AuthlibInjector {

InstallTask::InstallTask(MinecraftInstance* instance, QString serverUrl, QString mirrorBase)
    : m_instance(instance), m_serverUrl(std::move(serverUrl)), m_mirrorBase(std::move(mirrorBase))
{}

InstallTask::~InstallTask() = default;

bool InstallTask::abort()
{
    if (m_current) {
        m_current->abort();
    }
    if (isRunning()) {
        emitAborted();
    }
    return true;
}

void InstallTask::failWith(const QString& message)
{
    m_current.reset();
    // The failure of a request that was cancelled along with the task is expected, not a second outcome.
    if (!isRunning()) {
        return;
    }
    emitFailed(message);
}

void InstallTask::executeTask()
{
    if (m_mirrorBase.trimmed().isEmpty()) {
        // Same rule as the rest of the launcher: an empty address means no mirror, and the release
        // index only exists on one, so say so instead of quietly picking an address of our own.
        failWith(tr("authlib-injector is only published on the download mirror, but no mirror address is set. "
                    "Add one in Settings > APIs, or restore the default."));
        return;
    }
    fetchRelease();
}

void InstallTask::fetch(QUrl url, std::function<void(const QByteArray&)> ok, const QString& what)
{
    auto [request, body] = Net::Request::makeByteArray(url);
    request->setNetwork(APPLICATION->network());
    m_current = request;
    connect(request.get(), &Task::failed, this, [this, what](const QString& reason) { failWith(tr("Could not download %1: %2").arg(what, reason)); });
    connect(request.get(), &Task::succeeded, this, [body, ok = std::move(ok)]() { ok(*body); });
    request->start();
}

void InstallTask::fetchToFile(QUrl url, QString path, QString sha256, std::function<void()> ok, const QString& what)
{
    FS::ensureFolderPathExists(QFileInfo(path).absolutePath());
    auto request = Net::Request::makeFile(url, path);
    request->setNetwork(APPLICATION->network());
    request->addValidator(new Net::ChecksumValidator(QCryptographicHash::Sha256, sha256));
    m_current = request;
    connect(request.get(), &Task::failed, this, [this, what](const QString& reason) { failWith(tr("Could not download %1: %2").arg(what, reason)); });
    connect(request.get(), &Task::succeeded, this, [ok = std::move(ok)]() { ok(); });
    request->start();
}

void InstallTask::fetchRelease()
{
    setStatus(tr("Looking up the authlib-injector release"));
    auto base = m_mirrorBase.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    fetch(QUrl(base + QStringLiteral("/mirrors/authlib-injector/artifact/latest.json")),
          [this](const QByteArray& json) {
              m_release = parseRelease(json);
              if (m_release.version.isEmpty()) {
                  failWith(tr("The mirror did not return a usable authlib-injector release."));
                  return;
              }
              fetchJar();
          },
          tr("the release index"));
}

void InstallTask::fetchJar()
{
    /*
     * Agents are never fetched while a profile is updated, so the jar has to end up exactly where the launch will
     * look for it. Asking the library itself keeps the two in step, whatever it decides the storage prefix is.
     */
    Library library;
    library.setRawName(GradleSpecifier(AuthlibInjector::mavenName + m_release.version));
    QStringList native;
    QStringList native32;
    QStringList native64;
    QStringList jars;
    library.getApplicableFiles(m_instance->runtimeContext(), jars, native, native32, native64, QString());
    m_jarPath = jars.value(0);
    if (m_jarPath.isEmpty()) {
        failWith(tr("Could not work out where the authlib-injector jar belongs."));
        return;
    }

    setStatus(tr("Downloading authlib-injector %1").arg(m_release.version));

    // A reinstall should not fetch a jar we already have verified.
    if (QFileInfo::exists(m_jarPath)) {
        QFile existing(m_jarPath);
        if (existing.open(QIODevice::ReadOnly)) {
            auto digest = QString::fromLatin1(QCryptographicHash::hash(existing.readAll(), QCryptographicHash::Sha256).toHex());
            if (digest.compare(m_release.sha256, Qt::CaseInsensitive) == 0) {
                writeComponent();
                return;
            }
        }
    }

    fetchToFile(QUrl(m_release.downloadUrl), m_jarPath, m_release.sha256, [this] { writeComponent(); }, tr("authlib-injector"));
}

void InstallTask::writeComponent()
{
    // The release index may have answered after the player cancelled; nothing may be installed then.
    if (!isRunning()) {
        return;
    }
    QFile jar(m_jarPath);
    if (!jar.open(QIODevice::ReadOnly)) {
        failWith(tr("Could not read the downloaded authlib-injector jar."));
        return;
    }
    auto digest = QString::fromLatin1(QCryptographicHash::hash(jar.readAll(), QCryptographicHash::Sha1).toHex());
    auto packProfile = m_instance->getPackProfile();
    auto profile = buildComponent(m_release, m_serverUrl, packProfile->getComponentVersion("net.minecraft"),
                                  QUrl::fromLocalFile(m_jarPath).toString(), digest, QFileInfo(m_jarPath).size());

    // One private directory per install. A fixed name in the shared temp directory would be
    // overwritten by a second instance installing at the same time, and the component then copied
    // into this instance would point the game at the other server.
    QTemporaryDir stagingDir;
    if (!stagingDir.isValid()) {
        failWith(tr("Could not create a temporary directory for the authlib-injector component."));
        return;
    }
    auto staging = FS::PathCombine(stagingDir.path(), QStringLiteral("authlib-injector-component.json"));
    QSaveFile out(staging);
    if (!out.open(QIODevice::WriteOnly) || out.write(QJsonDocument(profile).toJson(QJsonDocument::Indented)) < 0 || !out.commit()) {
        failWith(tr("Could not write the authlib-injector component file."));
        return;
    }

    if (packProfile->getComponent(componentUid) && !packProfile->remove(componentUid)) {
        failWith(tr("Could not replace the authlib-injector component already in this instance."));
        return;
    }
    if (!packProfile->installComponents({ staging })) {
        failWith(tr("Could not add the authlib-injector component to the instance."));
        return;
    }

    m_current.reset();
    emitSucceeded();
}

}  // namespace AuthlibInjector
