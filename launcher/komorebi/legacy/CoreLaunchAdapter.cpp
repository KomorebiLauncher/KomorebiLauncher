// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/legacy/CoreLaunchAdapter.h"

#include <QHash>
#include <QPointer>

#include "Application.h"
#include "BaseInstance.h"

#include "komorebi/services/download/DownloadSource.h"
#include "komorebi/services/launch/InstanceLaunch.h"

namespace Komorebi::Legacy {

namespace {

/// The launches this run started, so the launcher can stop one without owning its process.
QHash<QString, QPointer<Services::InstanceLaunch>> s_launches;

}  // namespace

bool killInstance(const QString& instanceId)
{
    const auto task = s_launches.value(instanceId);
    if (!task)
        return false;
    task->kill();
    return true;
}

bool launchInstance(MinecraftInstance* instance, const MinecraftAccountPtr& account,
                    const std::function<void(const QString& logPath)>& onStarted,
                    const std::function<void(const QString& error)>& onFailed, QString* error,
                    const std::function<void(int exitCode)>& onFinished)
{
    if (!instance) {
        if (error)
            *error = QStringLiteral("no instance was selected");
        return false;
    }
    if (!account) {
        if (error)
            *error = QStringLiteral("no account is signed in");
        return false;
    }

    Services::InstanceLaunchOptions options;
    options.dataRoot = APPLICATION->dataRoot();
    options.instanceDirectory = instance->instanceRoot();
    options.minMemoryMb = instance->settings()->get("MinMemAlloc").toInt();
    options.maxMemoryMb = instance->settings()->get("MaxMemAlloc").toInt();
    options.source = Services::downloadSourceFromId(APPLICATION->settings()->get("DownloadSource").toString());

    Services::LaunchAccount who;
    who.username = account->profileName();
    who.uuid = account->profileId();
    who.accessToken = account->accessToken();

    auto* task = new Services::InstanceLaunch(APPLICATION->network(), APPLICATION);
    const QString id = instance->id();
    s_launches.insert(id, task);
    const auto forget = [id] { s_launches.remove(id); };

    QObject::connect(task, &Services::InstanceLaunch::started, APPLICATION, [onStarted](const QString& logPath) {
        if (onStarted)
            onStarted(logPath);
    });
    QObject::connect(task, &Services::InstanceLaunch::failed, APPLICATION, [onFailed, forget](const QString& reason) {
        forget();
        if (onFailed)
            onFailed(reason);
    });
    QObject::connect(task, &Services::InstanceLaunch::aborted, APPLICATION, [forget] { forget(); });
    QObject::connect(task, &Services::InstanceLaunch::gameFinished, APPLICATION, [onFinished, forget](int exitCode) {
        forget();
        if (onFinished)
            onFinished(exitCode);
    });
    task->start(options, who);
    return true;
}

}  // namespace Komorebi::Legacy
