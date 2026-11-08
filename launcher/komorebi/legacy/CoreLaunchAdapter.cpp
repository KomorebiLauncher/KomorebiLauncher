// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/legacy/CoreLaunchAdapter.h"

#include "Application.h"
#include "BaseInstance.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/auth/MinecraftAccount.h"

#include "komorebi/services/launch/InstanceLaunch.h"

namespace Komorebi::Legacy {

bool launchInstance(MinecraftInstance* instance, const MinecraftAccountPtr& account, QString* error)
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

    Services::LaunchAccount who;
    who.username = account->profileName();
    who.uuid = account->profileId();
    who.accessToken = account->accessToken();

    return Services::launchInstance(options, who, error);
}

}  // namespace Komorebi::Legacy
