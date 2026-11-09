// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Turning a version manifest into a command line.
 *
 *  A value in, a value out: no processes are started and nothing is written here, so the assembly can
 *  be checked without a game, a Java runtime or a network. Whether the files exist is the caller's
 *  business; this only has to be right about what it would run.
 */

#pragma once

#include <QString>
#include <QStringList>

#include "komorebi/core/version/VersionJson.h"

namespace Komorebi::Core {

/// Everything the command line needs that the manifest does not carry.
struct LaunchOptions {
    QString javaPath;           ///< the java executable to run
    QString gameDirectory;      ///< the instance directory
    QString assetsDirectory;    ///< where the asset index and objects live
    QString librariesDirectory;  ///< where the libraries live
    QString nativesDirectory;   ///< where the natives are unpacked
    QString clientJarPath;      ///< the version's own jar, last on the class path
    QString versionName;        ///< what the game is told its version is
    QString assetIndexName;     ///< overrides the manifest's asset index when set

    QString username;
    QString uuid;
    QString accessToken;
    QString userType = QStringLiteral("msa");
    QString clientId;
    QString xuid;

    int minMemoryMb = 512;
    int maxMemoryMb = 2048;
    QStringList extraJvmArguments;
    QStringList extraGameArguments;

    /// A server the game should join once it starts, as a launcher feature; port defaults to Minecraft's.
    QString serverAddress;
    int serverPort = 25565;
    /// A world to open directly, for versions whose launcher features allow it.
    QString worldToJoin;
};

/// A command line that can be started, or the reasons it cannot be.
struct LaunchPlan {
    QString program;
    QStringList arguments;
    QString workingDirectory;
    QStringList problems;  ///< empty when the plan is usable

    bool ok() const { return problems.isEmpty() && !program.isEmpty(); }
};

/// The version's own jar: <versionDirectory>/<versionId>.jar.
QString clientJarPath(const QString& versionDirectory, const QString& versionId);

/**
 * The class path for the libraries that apply on this platform, then the client jar.
 *
 * Natives jars are listed too: the game reads them from the class path and unpacks what it needs
 * into the natives directory, which is what the launcher's -Djava.library.path points at.
 */
QStringList classpathEntries(const VersionJson& merged, const LaunchOptions& options, const Platform& platform = Platform::current());

/// Assembles the command line. Problems are reported rather than thrown.
LaunchPlan buildLaunchPlan(const VersionJson& merged, const LaunchOptions& options, const Platform& platform = Platform::current());

}  // namespace Komorebi::Core
