// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Starting an instance with the core instead of the launcher's old launch path.
 *
 *  The pieces below the UI -- instance folder, components, version, Java, command line -- already
 *  exist; this is the join that turns them into a running game. It takes plain values so it can be
 *  called from the launcher's own classes without those classes leaking into the core.
 */

#pragma once

#include <QString>
#include <QStringList>

#include <optional>

#include "komorebi/core/launch/LaunchPlan.h"

namespace Komorebi::Services {

/// Who is playing: what the game is told about the account.
struct LaunchAccount {
    QString username;
    QString uuid;
    QString accessToken;
    QString userType = QStringLiteral("msa");
    QString clientId;
    QString xuid;
};

/// What the launch needs from the launcher: where everything lives, and how much memory to give it.
struct InstanceLaunchOptions {
    QString dataRoot;          ///< holds instances/, meta/, libraries/, assets/
    QString instanceDirectory;  ///< the instance to start
    int minMemoryMb = 512;
    int maxMemoryMb = 2048;
    QStringList extraJvmArguments;
};

/// Builds the command line for the instance, or explains why it cannot.
std::optional<Core::LaunchPlan> planInstance(const InstanceLaunchOptions& options, const LaunchAccount& account,
                                             QString* error = nullptr);

/**
 * Starts the instance and writes what the game says to <instance>/logs/core-launch.log.
 *
 * Returns once the process has started: the game outlives this call, and its output is in the log
 * rather than in a dialog that would block the window.
 */
bool launchInstance(const InstanceLaunchOptions& options, const LaunchAccount& account, QString* error = nullptr);

}  // namespace Komorebi::Services
