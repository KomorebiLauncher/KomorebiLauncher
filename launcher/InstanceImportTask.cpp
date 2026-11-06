// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "InstanceImportTask.h"

#include "Application.h"
#include "FileSystem.h"
#include "NullInstance.h"

#include "QObjectPtr.h"
#include "archive/ArchiveReader.h"
#include "archive/ExtractZipTask.h"
#include "icons/IconList.h"
#include "icons/IconUtils.h"

#include "modplatform/flame/FlameAPI.h"
#include "modplatform/flame/FlameInstanceCreationTask.h"
#include "modplatform/domestic/PackManifest.h"
#include "modplatform/modrinth/ModrinthInstanceCreationTask.h"
#include "modplatform/technic/TechnicPackProcessor.h"

#include "settings/INISettingsObject.h"
#include "tasks/Task.h"
#include "ui/dialogs/UntrustedModsDialog.h"

#include "net/ApiRequest.h"
#include "net/ChecksumValidator.h"
#include "net/NetJob.h"
#include "net/Request.h"

#include <QCryptographicHash>
#include <QHash>
#include <QDir>
#include <QDirListing>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QtConcurrentRun>
#include <memory>
#include <utility>

InstanceImportTask::InstanceImportTask(QUrl sourceUrl, bool trustedSource, QWidget* parent, QMap<QString, QString> extraInfo)
    : m_sourceUrl(std::move(sourceUrl)), m_trustedSource(trustedSource), m_extraInfo(std::move(extraInfo)), m_parent(parent)
{}

bool InstanceImportTask::abort()
{
    if (!canAbort()) {
        return false;
    }

    bool wasAborted = false;
    if (m_task) {
        wasAborted = m_task->abort();
    }
    return wasAborted;
}

void InstanceImportTask::executeTask()
{
    setAbortable(true);

    if (m_sourceUrl.isLocalFile()) {
        m_archivePath = m_sourceUrl.toLocalFile();
        processZipPack();
    } else {
        setStatus(tr("Downloading modpack:\n%1").arg(m_sourceUrl.toString()));

        downloadFromUrl();
    }
}

void InstanceImportTask::downloadFromUrl()
{
    const QString path(m_sourceUrl.host() + '/' + m_sourceUrl.path());

    auto entry = APPLICATION->metacache()->resolveEntry("general", path);
    entry->setStale(true);
    m_archivePath = entry->getFullPath();

    auto filesNetJob = makeShared<NetJob>(tr("Modpack download"), APPLICATION->network());
    filesNetJob->addNetAction(Net::ApiRequest::makeCached(m_sourceUrl, entry));

    connect(filesNetJob.get(), &NetJob::succeeded, this, &InstanceImportTask::processZipPack);
    connect(filesNetJob.get(), &NetJob::progress, this, &InstanceImportTask::setProgress);
    connect(filesNetJob.get(), &NetJob::stepProgress, this, &InstanceImportTask::propagateStepProgress);
    connect(filesNetJob.get(), &NetJob::failed, this, &InstanceImportTask::emitFailed);
    connect(filesNetJob.get(), &NetJob::aborted, this, &InstanceImportTask::emitAborted);
    m_task.reset(filesNetJob);
    filesNetJob->start();
}
namespace {

QString cleanPath(const QString& path)
{
    if (path == ".") {
        return QString();
    }
    QString result = path;
    if (result.startsWith("./")) {
        result = result.mid(2);
    }
    return result;
}
}  // namespace

void InstanceImportTask::processZipPack()
{
    setStatus(tr("Attempting to determine instance type"));
    QDir extractDir(m_stagingPath);
    qDebug() << "Attempting to create instance from" << m_archivePath;

    // open the zip and find relevant files in it
    MMCZip::ArchiveReader packZip(m_archivePath);
    qDebug() << "Attempting to determine instance type";

    QString root;
    // NOTE: Prioritize modpack platforms that aren't searched for recursively.
    // Especially Flame has a very common filename for its manifest, which may appear inside overrides for example
    // https://docs.modrinth.com/docs/modpacks/format_definition/#storage
    auto detectInstance = [this, &extractDir, &root](MMCZip::ArchiveReader::File* f) -> Result<bool> {
        if (!isRunning()) {
            return true;
        }
        auto fileName = f->filename();
        if (fileName == "modrinth.index.json") {
            // process as Modrinth pack
            qDebug() << "Modrinth:" << true;
            m_modpackType = ModpackType::Modrinth;
            return true;
        } else if (fileName == "bin/modpack.jar" || fileName == "bin/version.json") {
            // process as Technic pack
            qDebug() << "Technic:" << true;
            extractDir.mkpath("minecraft");
            extractDir.cd("minecraft");
            m_modpackType = ModpackType::Technic;
            return true;
        } else if (fileName == "manifest.json") {
            qDebug() << "Flame:" << true;
            m_modpackType = ModpackType::Flame;
            return true;
        } else if (fileName == "mcbbs.packmeta") {
            // Only the MCBBS format has this file, so it identifies the pack on its own.
            qDebug() << "MCBBS:" << true;
            m_modpackType = ModpackType::Mcbbs;
            return true;
        } else if (fileName == "modpack.json") {
            // HMCL's export marker. Whether the instance manifest next to it exists is checked
            // during the import, so a pack that merely ships a file with this name fails loudly
            // instead of being silently mangled.
            qDebug() << "HMCL:" << true;
            m_modpackType = ModpackType::HMCL;
            return true;
        } else if (QFileInfo fileInfo(fileName); fileInfo.fileName() == "instance.cfg") {
            qDebug() << "MultiMC:" << true;
            m_modpackType = ModpackType::MultiMC;
            root = cleanPath(fileInfo.path());
            return true;
        }
        QCoreApplication::processEvents();
        return false;
    };
    if (!packZip.parse(detectInstance)) {
        emitFailed(tr("Unable to open supplied modpack zip file."));
        return;
    }
    if (m_modpackType == ModpackType::Unknown) {
        emitFailed(tr("Archive does not contain a recognized modpack type."));
        return;
    }
    setStatus(tr("Extracting modpack"));

    // make sure we extract just the pack
    auto zipTask = makeShared<MMCZip::ExtractZipTask>(m_archivePath, extractDir, root);

    auto progressStep = std::make_shared<TaskStepProgress>();
    connect(zipTask.get(), &Task::finished, this, [this, progressStep] {
        progressStep->state = TaskStepState::Succeeded;
        stepProgress(*progressStep);
    });

    connect(zipTask.get(), &Task::succeeded, this, &InstanceImportTask::extractFinished, Qt::QueuedConnection);
    connect(zipTask.get(), &Task::aborted, this, &InstanceImportTask::emitAborted);
    connect(zipTask.get(), &Task::failed, this, [this, progressStep](QString reason) {
        progressStep->state = TaskStepState::Failed;
        stepProgress(*progressStep);
        emitFailed(std::move(reason));
    });
    connect(zipTask.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);

    connect(zipTask.get(), &Task::progress, this, [this, progressStep](qint64 current, qint64 total) {
        progressStep->update(current, total);
        stepProgress(*progressStep);
    });
    connect(zipTask.get(), &Task::status, this, [this, progressStep](QString status) {
        progressStep->status = std::move(status);
        stepProgress(*progressStep);
    });
    connect(zipTask.get(), &Task::warningLogged, this, [this](const QString& line) { m_Warnings.append(line); });
    m_task.reset(zipTask);
    zipTask->start();
}

void InstanceImportTask::extractFinished()
{
    setAbortable(false);

    qDebug() << "Fixing permissions for extracted pack files...";
    for (const auto& file : QDirListing(m_stagingPath, QDirListing::IteratorFlag::ResolveSymlinks | QDirListing::IteratorFlag::Recursive)) {
        auto permissions = QFile::permissions(file.absoluteFilePath());
        auto origPermissions = permissions;
        if (file.isDir()) {
            // Folder +rwx for current user
            permissions |= QFileDevice::Permission::ReadUser | QFileDevice::Permission::WriteUser | QFileDevice::Permission::ExeUser;
        } else {
            // File +rw for current user
            permissions |= QFileDevice::Permission::ReadUser | QFileDevice::Permission::WriteUser;
        }
        if (origPermissions != permissions) {
            if (!QFile::setPermissions(file.absoluteFilePath(), permissions)) {
                logWarning(tr("Could not fix permissions for %1").arg(file.absoluteFilePath()));
            } else {
                qDebug() << "Fixed" << file.absoluteFilePath();
            }
        }
    }

    switch (m_modpackType) {
        case ModpackType::MultiMC:
            processMultiMC();
            return;
        case ModpackType::Technic:
            processTechnic();
            return;
        case ModpackType::Flame:
            processFlame();
            return;
        case ModpackType::Modrinth:
            processModrinth();
            return;
        case ModpackType::HMCL:
        case ModpackType::Mcbbs:
            processDomestic();
            return;
        case ModpackType::Unknown:
            emitFailed(tr("Archive does not contain a recognized modpack type."));
            return;
    }
}

namespace {

bool installIcon(const QString& root, const QString& instIconKey)
{
    auto importIconPath = IconUtils::findBestIconIn(root, instIconKey);
    if (importIconPath.isNull() || !QFile::exists(importIconPath)) {
        importIconPath = IconUtils::findBestIconIn(root, "icon.png");
    }
    if (importIconPath.isNull() || !QFile::exists(importIconPath)) {
        importIconPath = IconUtils::findBestIconIn(FS::PathCombine(root, "overrides"), "icon.png");
    }
    if (!importIconPath.isNull() && QFile::exists(importIconPath)) {
        // import icon
        auto* iconList = APPLICATION->icons();
        if (iconList->iconFileExists(instIconKey)) {
            iconList->deleteIcon(instIconKey);
        }
        iconList->installIcon(importIconPath, instIconKey + "." + QFileInfo(importIconPath).suffix());
        return true;
    }
    return false;
}
}  // namespace

void InstanceImportTask::processFlame()
{
    shared_qobject_ptr<FlameCreationTask> instCreationTask = nullptr;
    if (!m_extraInfo.isEmpty()) {
        auto packIdIt = m_extraInfo.constFind("pack_id");
        Q_ASSERT(packIdIt != m_extraInfo.constEnd());
        const auto& packId = packIdIt.value();

        auto packVersionIdIt = m_extraInfo.constFind("pack_version_id");
        Q_ASSERT(packVersionIdIt != m_extraInfo.constEnd());
        const auto& packVersionId = packVersionIdIt.value();

        QString originalInstanceId;
        auto originalInstanceIdIt = m_extraInfo.constFind("original_instance_id");
        if (originalInstanceIdIt != m_extraInfo.constEnd()) {
            originalInstanceId = originalInstanceIdIt.value();
        }

        instCreationTask = makeShared<FlameCreationTask>(m_stagingPath, m_trustedSource, m_globalSettings, m_parent, packId, packVersionId,
                                                         originalInstanceId);
    } else {
        // FIXME: Find a way to get IDs in directly imported ZIPs
        instCreationTask = makeShared<FlameCreationTask>(m_stagingPath, m_trustedSource, m_globalSettings, m_parent, QString(), QString());
    }

    instCreationTask->setName(modifiedName());
    instCreationTask->setOriginalName(originalName(), version());
    // if the icon was specified by user, use that. otherwise pull icon from the pack
    if (m_instIcon == "default") {
        auto iconKey = QString("Flame_%1_Icon").arg(name());

        if (installIcon(m_stagingPath, iconKey)) {
            m_instIcon = iconKey;
        }
    }
    instCreationTask->setIcon(m_instIcon);
    instCreationTask->setGroup(m_instGroup);
    instCreationTask->setConfirmUpdate(shouldConfirmUpdate());

    auto weak = instCreationTask.toWeakRef();
    connect(instCreationTask.get(), &Task::succeeded, this, [this, weak] {
        if (auto sp = weak.lock()) {
            setOverride(sp->shouldOverride(), sp->originalInstanceID());
        }
        emitSucceeded();
    });
    connect(instCreationTask.get(), &Task::failed, this, &InstanceImportTask::emitFailed);
    connect(instCreationTask.get(), &Task::progress, this, &InstanceImportTask::setProgress);
    connect(instCreationTask.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);
    connect(instCreationTask.get(), &Task::status, this, &InstanceImportTask::setStatus);
    connect(instCreationTask.get(), &Task::details, this, &InstanceImportTask::setDetails);

    connect(instCreationTask.get(), &Task::aborted, this, &InstanceImportTask::emitAborted);
    connect(instCreationTask.get(), &Task::abortStatusChanged, this, &Task::setAbortable);
    connect(instCreationTask.get(), &Task::abortButtonTextChanged, this, &Task::setAbortButtonText);

    connect(instCreationTask.get(), &Task::warningLogged, this, [this](const QString& line) { m_Warnings.append(line); });

    m_task.reset(instCreationTask);
    setAbortable(true);
    m_task->start();
}

void InstanceImportTask::processTechnic()
{
    shared_qobject_ptr<Technic::TechnicPackProcessor> packProcessor{ new Technic::TechnicPackProcessor };
    connect(packProcessor.get(), &Technic::TechnicPackProcessor::succeeded, this, &InstanceImportTask::emitSucceeded);
    connect(packProcessor.get(), &Technic::TechnicPackProcessor::failed, this, &InstanceImportTask::emitFailed);
    packProcessor->run(m_globalSettings, name(), m_instIcon, m_stagingPath);
}

void InstanceImportTask::processMultiMC()
{
    QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
    auto instanceSettings = std::make_unique<INISettingsObject>(configPath);

    NullInstance instance(m_globalSettings, std::move(instanceSettings), m_stagingPath);

    // reset time played on import... because packs.
    instance.resetTimePlayed();

    // UUID is carried over on export, but this is a distinct instance, so give it its own
    instance.regenerateUuid();

    // set a new nice name
    instance.setName(name());

    // if the icon was specified by user, use that. otherwise pull icon from the pack
    if (m_instIcon != "default") {
        instance.setIconKey(m_instIcon);
    } else {
        m_instIcon = instance.iconKey();

        installIcon(instance.instanceRoot(), m_instIcon);
    }
    emitSucceeded();
}

void InstanceImportTask::processModrinth()
{
    shared_qobject_ptr<ModrinthCreationTask> instCreationTask = nullptr;
    if (!m_extraInfo.isEmpty()) {
        auto packIdIt = m_extraInfo.constFind("pack_id");
        Q_ASSERT(packIdIt != m_extraInfo.constEnd());
        const auto& packId = packIdIt.value();

        QString packVersionId;
        auto packVersionIdIt = m_extraInfo.constFind("pack_version_id");
        if (packVersionIdIt != m_extraInfo.constEnd()) {
            packVersionId = packVersionIdIt.value();
        }

        QString originalInstanceId;
        auto originalInstanceIdIt = m_extraInfo.constFind("original_instance_id");
        if (originalInstanceIdIt != m_extraInfo.constEnd()) {
            originalInstanceId = originalInstanceIdIt.value();
        }

        instCreationTask = makeShared<ModrinthCreationTask>(m_stagingPath, m_trustedSource, m_globalSettings, m_parent, packId,
                                                            packVersionId, originalInstanceId);
    } else {
        QString packId;
        if (!m_sourceUrl.isEmpty()) {
            static const QRegularExpression s_regex(R"(data\/([^\/]*)\/versions)");
            packId = s_regex.match(m_sourceUrl.toString()).captured(1);
        }

        // FIXME: Find a way to get the ID in directly imported ZIPs
        instCreationTask = makeShared<ModrinthCreationTask>(m_stagingPath, m_trustedSource, m_globalSettings, m_parent, packId);
    }

    instCreationTask->setName(modifiedName());
    instCreationTask->setOriginalName(originalName(), version());
    // if the icon was specified by user, use that. otherwise pull icon from the pack
    if (m_instIcon == "default") {
        auto iconKey = QString("Modrinth_%1_Icon").arg(name());

        if (installIcon(m_stagingPath, iconKey)) {
            m_instIcon = iconKey;
        }
    }
    instCreationTask->setIcon(m_instIcon);
    instCreationTask->setGroup(m_instGroup);
    instCreationTask->setConfirmUpdate(shouldConfirmUpdate());

    auto weak = instCreationTask.toWeakRef();
    connect(instCreationTask.get(), &Task::succeeded, this, [this, weak] {
        if (auto sp = weak.lock()) {
            setOverride(sp->shouldOverride(), sp->originalInstanceID());
        }
        emitSucceeded();
    });
    connect(instCreationTask.get(), &Task::failed, this, &InstanceImportTask::emitFailed);
    connect(instCreationTask.get(), &Task::progress, this, &InstanceImportTask::setProgress);
    connect(instCreationTask.get(), &Task::stepProgress, this, &InstanceImportTask::propagateStepProgress);
    connect(instCreationTask.get(), &Task::status, this, &InstanceImportTask::setStatus);
    connect(instCreationTask.get(), &Task::details, this, &InstanceImportTask::setDetails);

    connect(instCreationTask.get(), &Task::aborted, this, &InstanceImportTask::emitAborted);
    connect(instCreationTask.get(), &Task::abortStatusChanged, this, &Task::setAbortable);
    connect(instCreationTask.get(), &Task::abortButtonTextChanged, this, &Task::setAbortButtonText);

    connect(instCreationTask.get(), &Task::warningLogged, this, [this](const QString& line) { m_Warnings.append(line); });

    m_task.reset(instCreationTask);
    setAbortable(true);
    m_task->start();
}

namespace {

/** The pack component id each loader name maps to. */
auto loaderUid(const QString& loader) -> QString
{
    if (loader == QLatin1String("fabric"))
        return QStringLiteral("net.fabricmc.fabric-loader");
    if (loader == QLatin1String("quilt"))
        return QStringLiteral("org.quiltmc.quilt-loader");
    if (loader == QLatin1String("forge"))
        return QStringLiteral("net.minecraftforge");
    if (loader == QLatin1String("neoforge"))
        return QStringLiteral("net.neoforged");
    return {};
}

}  // namespace

void InstanceImportTask::processDomestic()
{
    const bool isMcbbs = m_modpackType == ModpackType::Mcbbs;

    auto readEntry = [this](const QString& name) -> QByteArray {
        QFile file(FS::PathCombine(m_stagingPath, name));
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    };

    Domestic::PackDescription pack;
    QString error;

    if (isMcbbs) {
        const auto manifest = readEntry(QStringLiteral("mcbbs.packmeta"));
        if (manifest.isEmpty()) {
            emitFailed(tr("The pack does not contain an mcbbs.packmeta manifest."));
            return;
        }
        if (!Domestic::parseMcbbs(manifest, pack, &error)) {
            emitFailed(tr("Could not read the pack manifest:\n%1").arg(error));
            return;
        }
    } else {
        const auto manifest = readEntry(QStringLiteral("modpack.json"));
        if (manifest.isEmpty()) {
            emitFailed(tr("The pack does not contain an HMCL modpack.json manifest."));
            return;
        }
        const auto instance = readEntry(QStringLiteral("minecraft/pack.json"));
        if (instance.isEmpty()) {
            emitFailed(tr("The pack does not contain the HMCL instance manifest (minecraft/pack.json), so its game "
                          "version cannot be determined."));
            return;
        }
        if (!Domestic::parseHmcl(manifest, instance, pack, &error)) {
            emitFailed(tr("Could not read the pack manifest:\n%1").arg(error));
            return;
        }
    }

    for (const auto& warning : pack.warnings)
        logWarning(warning);

    if (pack.gameVersion.isEmpty()) {
        emitFailed(tr("The pack manifest does not name a Minecraft version."));
        return;
    }

    // Both formats keep the game directory under a prefix; the rest of the archive is the manifest.
    // Flattening it here is what lets the rest of the import be the MultiMC path.
    const auto gameDir = FS::PathCombine(m_stagingPath, pack.overridesPrefix);
    if (!QFileInfo::exists(gameDir)) {
        emitFailed(tr("The pack does not contain the expected %1 folder.").arg(pack.overridesPrefix));
        return;
    }
    const auto entries = QDir(gameDir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const auto& entry : entries) {
        const auto target = FS::PathCombine(m_stagingPath, entry.fileName());
        if (QFileInfo::exists(target)) {
            // The manifest files sit at the root under names the game folder also uses (modpack.json
            // is the realistic case), so the copy that belongs to the game directory wins.
            logWarning(tr("Both the archive root and the game folder contain %1; keeping the one inside the game folder.")
                           .arg(entry.fileName()));
            FS::deletePath(target);
        }
        if (!FS::move(entry.absoluteFilePath(), target)) {
            emitFailed(tr("Could not move %1 out of the pack's game folder.").arg(entry.fileName()));
            return;
        }
    }
    QDir().rmdir(gameDir);

    // A pack the user picked from their own disk carries mods nobody vetted, which is the same
    // situation Flame imports warn about; using the same dialog keeps the two paths consistent
    // instead of inventing a second opinion for the same risk.
    if (!m_trustedSource) {
        QStringList untrustedMods;
        const auto modsPath = FS::PathCombine(m_stagingPath, QStringLiteral("mods"));
        if (QDir(modsPath).exists()) {
            for (const auto& entry :
                 QDirListing(modsPath, QDirListing::IteratorFlag::FilesOnly | QDirListing::IteratorFlag::ResolveSymlinks)) {
                untrustedMods.append(QStringLiteral("mods/") + entry.fileName());
            }
        }
        if (!untrustedMods.isEmpty()) {
            UntrustedModsDialog dialog{ untrustedMods, m_parent };
            if (dialog.exec() != QDialog::Accepted) {
                emitAborted();
                return;
            }
        }
    }

    // A minimal instance.cfg is enough to say what this folder is; processMultiMC fills in the rest.
    {
        QSettings config(FS::PathCombine(m_stagingPath, QStringLiteral("instance.cfg")), QSettings::IniFormat);
        config.setValue(QStringLiteral("InstanceType"), QStringLiteral("OneSix"));
        config.sync();
    }

    QJsonArray components;
    components.append(QJsonObject{ { QStringLiteral("uid"), QStringLiteral("net.minecraft") }, { QStringLiteral("version"), pack.gameVersion } });
    const auto uid = loaderUid(pack.loader);
    if (!uid.isEmpty() && !pack.loaderVersion.isEmpty()) {
        components.append(QJsonObject{ { QStringLiteral("uid"), uid }, { QStringLiteral("version"), pack.loaderVersion } });
    } else if (!pack.loader.isEmpty()) {
        logWarning(tr("The pack names the %1 loader but not its version, so it will not be installed automatically.")
                       .arg(pack.loader));
    }

    QFile profile(FS::PathCombine(m_stagingPath, QStringLiteral("mmc-pack.json")));
    if (!profile.open(QIODevice::WriteOnly)) {
        emitFailed(tr("Could not write mmc-pack.json into the instance folder."));
        return;
    }
    profile.write(QJsonDocument(QJsonObject{ { QStringLiteral("formatVersion"), 1 }, { QStringLiteral("components"), components } }).toJson());
    profile.close();

    if (pack.files.isEmpty()) {
        processMultiMC();
        return;
    }

    // The archives carry most of their content, but a pack may also list files it expects from a
    // service. Those are fetched here so the instance is complete when it is handed over.
    QStringList curseforgeIds;
    for (const auto& file : pack.files) {
        if (file.curseforgeFileId != 0)
            curseforgeIds.append(QString::number(file.curseforgeFileId));
    }

    if (!curseforgeIds.isEmpty()) {
        setStatus(tr("Resolving the files the pack expects from CurseForge"));
        auto [resolve, response] = FlameAPI::getFiles(curseforgeIds);
        connect(resolve.get(), &Task::succeeded, this, [this, response, pack] {
            // The response is a list of file objects; the file name and the download URL are all
            // this needs, and the pack already said where each one belongs.
            const auto document = QJsonDocument::fromJson(*response);
            const auto entries = document.isObject() ? document.object().value(QLatin1String("data")).toArray() : QJsonArray();
            if (entries.isEmpty()) {
                emitFailed(tr("CurseForge returned no information about the files this pack lists."));
                return;
            }

            QHash<QString, QUrl> urlByFileId;
            QHash<QString, QString> nameByFileId;
            for (const auto& value : entries) {
                const auto object = value.toObject();
                const auto id = QString::number(static_cast<qint64>(object.value(QLatin1String("id")).toDouble()));
                urlByFileId.insert(id, QUrl(object.value(QLatin1String("downloadUrl")).toString(), QUrl::TolerantMode));
                nameByFileId.insert(id, object.value(QLatin1String("fileName")).toString());
            }

            auto job = makeShared<NetJob>(tr("Pack files"), APPLICATION->network());
            int missing = 0;
            for (const auto& file : pack.files) {
                const auto id = QString::number(file.curseforgeFileId);
                if (file.curseforgeFileId != 0) {
                    const auto url = urlByFileId.value(id);
                    if (!url.isValid() || url.isEmpty()) {
                        ++missing;
                        continue;
                    }
                    // The manifest may omit the destination; CurseForge's own file name is then the
                    // best guess, and it is what the launcher that made the pack would have used.
                    auto destination = file.path;
                    if (destination.isEmpty())
                        destination = QStringLiteral("mods/") + nameByFileId.value(id);
                    auto download = Net::Request::makeFile(url, FS::PathCombine(m_stagingPath, destination));
                    if (!file.sha1.isEmpty())
                        download->addValidator(
                            new Net::ChecksumValidator(QCryptographicHash::Sha1, QByteArray::fromHex(file.sha1.toLatin1())));
                    job->addNetAction(download);
                } else if (!file.url.isEmpty()) {
                    auto download = Net::Request::makeFile(QUrl(file.url, QUrl::TolerantMode), FS::PathCombine(m_stagingPath, file.path));
                    if (!file.sha1.isEmpty())
                        download->addValidator(
                            new Net::ChecksumValidator(QCryptographicHash::Sha1, QByteArray::fromHex(file.sha1.toLatin1())));
                    job->addNetAction(download);
                }
            }

            if (job->size() == 0) {
                emitFailed(tr("None of the files this pack lists could be downloaded."));
                return;
            }
            if (missing > 0) {
                logWarning(tr("%1 file(s) in the pack are no longer available from CurseForge and were skipped.").arg(missing));
            }

            connect(job.get(), &Task::failed, this, [this](const QString& reason) {
                emitFailed(tr("Could not download the files this pack lists:\n%1").arg(reason));
            });
            connect(job.get(), &Task::aborted, this, &InstanceImportTask::emitAborted);
            connect(job.get(), &Task::progress, this, &InstanceImportTask::setProgress);
            connect(job.get(), &Task::status, this, &InstanceImportTask::setStatus);
            connect(job.get(), &Task::succeeded, this, &InstanceImportTask::processMultiMC);

            m_task = job;
            setAbortable(true);
            job->start();
        });
        connect(resolve.get(), &Task::failed, this, [this](const QString& reason) {
            emitFailed(tr("Could not ask CurseForge about the files this pack lists:\n%1").arg(reason));
        });
        connect(resolve.get(), &Task::aborted, this, &InstanceImportTask::emitAborted);

        m_task = resolve;
        setAbortable(true);
        resolve->start();
        return;
    }

    // No CurseForge references: only files the manifest gives a URL for.
    auto job = makeShared<NetJob>(tr("Pack files"), APPLICATION->network());
    for (const auto& file : pack.files) {
        if (file.url.isEmpty())
            continue;
        auto download = Net::Request::makeFile(QUrl(file.url, QUrl::TolerantMode), FS::PathCombine(m_stagingPath, file.path));
        if (!file.sha1.isEmpty())
            download->addValidator(new Net::ChecksumValidator(QCryptographicHash::Sha1, QByteArray::fromHex(file.sha1.toLatin1())));
        job->addNetAction(download);
    }
    if (job->size() == 0) {
        processMultiMC();
        return;
    }
    connect(job.get(), &Task::failed, this, [this](const QString& reason) {
        emitFailed(tr("Could not download the files this pack lists:\n%1").arg(reason));
    });
    connect(job.get(), &Task::aborted, this, &InstanceImportTask::emitAborted);
    connect(job.get(), &Task::progress, this, &InstanceImportTask::setProgress);
    connect(job.get(), &Task::status, this, &InstanceImportTask::setStatus);
    connect(job.get(), &Task::succeeded, this, &InstanceImportTask::processMultiMC);

    m_task = job;
    setAbortable(true);
    job->start();
}