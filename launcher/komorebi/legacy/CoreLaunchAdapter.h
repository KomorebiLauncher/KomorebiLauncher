// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The one place where the core's launch path meets the launcher's own types.
 *
 *  The core is written against plain values; an instance and an account are the launcher's classes.
 *  This file is the join, and the only file in the new layer that includes them, so the day the old
 *  classes go away this is what has to be rewritten.
 */

#pragma once

#include <QString>

#include <functional>

#include "komorebi/services/auth/AuthSession.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/auth/MinecraftAccount.h"

namespace Komorebi::Legacy {

/**
 * Starts the instance through the core.
 *
 * `coreAccount` is the Komorebi account to play as, when one is configured; an empty uuid means the
 * launcher's own `account` is used instead. A stored Komorebi session has no access token -- those
 * live hours, not months -- so a launch renews it first, without blocking the window.
 *
 * Returns false and fills `error` when the launch cannot even begin; once it is running, the game
 * appears asynchronously and either callback is called exactly once. `onStarted` receives the path of
 * the log the game writes to.
 */
bool launchInstance(MinecraftInstance* instance, const MinecraftAccountPtr& account,
                    const Komorebi::Services::AuthSession& coreAccount,
                    const std::function<void(const QString& logPath)>& onStarted,
                    const std::function<void(const QString& error)>& onFailed, QString* error = nullptr,
                    const std::function<void(int exitCode)>& onFinished = nullptr,
                    const std::function<void()>& onAborted = nullptr,
                    const std::function<void(const QString& status)>& onStatus = nullptr);

/// Stops the launch, or the game, that was started for this instance. False when it was not started here.
bool killInstance(const QString& instanceId);

}  // namespace Komorebi::Legacy
