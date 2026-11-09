// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/legacy/CoreLaunchAdapter.h"

#include "Application.h"
#include "BaseInstance.h"

#include "komorebi/services/download/DownloadSource.h"
#include "komorebi/services/launch/InstanceLaunch.h"

namespace Komorebi::Legacy {

bool launchInstance(MinecraftInstance* instance, const MinecraftAccountPtr& account,
                    const std::function<void(const QString& logPath)>& onStarted,
                    const std::function<void(const QString& error)>& onFailed, QString* error)
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
    QObject::connect(task, &Services::InstanceLaunch::started, APPLICATION, [onStarted](const QString& logPath) {
        if (onStarted)
            onStarted(logPath);
    });
    QObject::connect(task, &Services::InstanceLaunch::failed, APPLICATION, [onFailed](const QString& reason) {
        if (onFailed)
            onFailed(reason);
    });
    task->start(options, who);
    return true;
}

}  // namespace Komorebi::Legacy
