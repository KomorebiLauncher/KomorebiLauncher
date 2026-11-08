// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Finding Java, and knowing which one it found.
 *
 *  A runtime is a value here: a path and what the runtime says about itself. Discovery reads the
 *  places runtimes live in, but parsing and choosing are pure, so the rules can be tested without a
 *  JDK on the machine.
 */

#pragma once

#include <QList>
#include <QString>

#include <optional>

namespace Komorebi::Services {

/// One Java installation, as the launcher sees it.
struct JavaRuntime {
    QString path;          ///< the executable to run
    QString home;          ///< the installation directory
    QString version;       ///< "17.0.9", as the runtime reports it
    int major = 0;         ///< 8, 17, 21 -- normalized, so 1.8 is 8
    QString architecture;  ///< x86_64, aarch64, ...
    QString vendor;        ///< the implementor, when the runtime says

    bool isValid() const { return !path.isEmpty() && major > 0; }
};

/// The executable inside a runtime's home: bin/java (java.exe on Windows).
QString executableIn(const QString& home);

/// "1.8.0_202" is 8, "17.0.9" is 17, "21" is 21; anything unparseable is 0.
int majorFromVersion(const QString& version);

/**
 * Reads a JDK's `release` file -- JAVA_VERSION="17.0.9", OS_ARCH="x86_64", IMPLEMENTOR="..." --
 * which every JDK 9 and later writes, and earlier ones often do too.
 */
std::optional<JavaRuntime> parseRelease(const QString& text, const QString& home, const QString& executable = {});

/// Where the launcher keeps the runtimes it downloaded, in the layout Mojang publishes them.
QList<JavaRuntime> scanMojangRuntimes(const QString& dataRoot);

/// Runtimes in the places a machine usually keeps them: JAVA_HOME, the platform's install folders.
QList<JavaRuntime> scanInstalledRuntimes();

/// The two above, without duplicates (same executable).
QList<JavaRuntime> discover(const QString& dataRoot);

/**
 * The runtime to use for a version that asks for a Java major.
 *
 * An exact match wins; otherwise the smallest major above the one asked for, because a newer runtime
 * can run an older game and an older one cannot run a newer game; the largest below is the last
 * resort. Callers decide whether a last resort is acceptable.
 */
std::optional<JavaRuntime> choose(const QList<JavaRuntime>& runtimes, int requiredMajor);

}  // namespace Komorebi::Services
