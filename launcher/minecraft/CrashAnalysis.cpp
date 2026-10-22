// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "CrashAnalysis.h"

#include <QRegularExpression>

#include <utility>

namespace {

/**
 * The lines are matched as substrings because every one of them arrives behind a log4j prefix like
 * "[14:28:38] [main/ERROR] [ne.mi.fm.lo.ModSorter/LOADING]: ", and the stack traces indent with tabs.
 */
auto indexOfLast(const QStringList& lines, const QString& needle, int from = 0, int to = -1) -> int
{
    int last = lines.size() - 1;
    if (last < from) {
        return -1;
    }
    for (int i = qMin(to < 0 ? last : to, last); i >= from; i--) {
        if (lines.at(i).contains(needle)) {
            return i;
        }
    }
    return -1;
}

auto lineAt(const QStringList& lines, int index) -> QString
{
    return index < 0 ? QString() : lines.at(index).trimmed();
}

auto findLine(const QStringList& lines, std::initializer_list<const char*> needles) -> QString
{
    for (auto needle : needles) {
        auto found = lineAt(lines, indexOfLast(lines, QString::fromUtf8(needle)));
        if (!found.isEmpty()) {
            return found;
        }
    }
    return {};
}

/** Reads the integer out of "class file version 61.0" style wording. */
auto classVersionIn(const QString& text, const QString& marker) -> int
{
    QRegularExpression re(QString::fromUtf8("%1 (\\d+)\\.\\d+").arg(marker));
    auto match = re.match(text);
    if (!match.hasMatch()) {
        return -1;
    }
    return match.captured(1).toInt();
}

}  // namespace

auto CrashAnalysis::javaReleaseForClassVersion(int classVersion) -> QString
{
    if (classVersion < 45 || classVersion > 70) {
        return {};
    }
    auto release = classVersion - 44;
    return release <= 8 ? QStringLiteral("1.%1").arg(release) : QString::number(release);
}

auto CrashAnalysis::analyze(const QStringList& lines) -> QList<CrashFinding>
{
    QList<CrashFinding> findings;

    auto heap = findLine(lines, { "java.lang.OutOfMemoryError: Java heap space" });
    auto metaspace = findLine(lines, { "java.lang.OutOfMemoryError: Metaspace" });
    auto anyOom = findLine(lines, { "java.lang.OutOfMemoryError" });

    if (!heap.isEmpty()) {
        findings.append({ tr("The game ran out of memory"),
                          heap,
                          tr("Raise this instance's maximum allocated memory, and ask less of the heap: lower the render "
                             "distance, turn off fancy graphics, drop resource packs with large textures. If it still runs out "
                             "with a lot of headroom, one mod is holding on to memory - take out the most recently added mods first.") });
    } else if (!metaspace.isEmpty()) {
        findings.append({ tr("The game ran out of class space"),
                          metaspace,
                          tr("Metaspace holds the classes of everything that is loaded, so it fills up with the number of mods "
                             "rather than the size of the heap. Add -XX:MaxMetaspaceSize=512M to this instance's Java arguments, "
                             "or remove mods.") });
    } else if (!anyOom.isEmpty()) {
        findings.append({ tr("The game ran out of memory"),
                          anyOom,
                          tr("Raise this instance's maximum allocated memory. If the line above names something other than the "
                             "heap, it is the mod that asked for the memory, not the amount.") });
    }

    // The JVM was handed a heap it was never going to get, so the game did not start at all.
    if (auto refused = findLine(lines, { "Could not reserve enough space for object heap", "Invalid maximum heap size" });
        !refused.isEmpty()) {
        findings.append({ tr("The memory requested cannot be given"),
                          refused,
                          tr("The heap this instance asks for is bigger than what this Java can hand over. Lower the maximum "
                             "memory - the settings page suggests a number for this machine - and check that the Java in use is "
                             "64-bit, since a 32-bit one stops being useful somewhere above 1.5 GiB.") });
    }

    auto classVersion = findLine(lines, { "java.lang.UnsupportedClassVersionError", "Unsupported major.minor version" });
    if (!classVersion.isEmpty()) {
        auto wanted = javaReleaseForClassVersion(classVersionIn(classVersion, QStringLiteral("class file version")));
        auto present = javaReleaseForClassVersion(classVersionIn(classVersion, QStringLiteral("versions up to")));
        // Older loaders report the same thing as "Unsupported major.minor version 52.0" and name the Java in use elsewhere.
        if (present.isEmpty()) {
            present = javaReleaseForClassVersion(classVersionIn(classVersion, QStringLiteral("major.minor version")));
            std::swap(wanted, present);
        }
        auto advice = tr("Something here was compiled for %1, but this instance is launching with %2. Set a newer Java in this "
                         "instance's Java settings, or remove the mod that needs it.")
                          .arg(wanted.isEmpty() ? tr("a newer Java") : tr("Java %1").arg(wanted),
                               present.isEmpty() ? tr("an older Java") : tr("Java %1").arg(present));
        findings.append({ tr("A mod needs a newer Java than the one in use"), classVersion, advice });
    }

    // A class that failed to initialize once is reported as missing every time after that, so the first complaint
    // is the one that says why.
    auto uninitialized = findLine(lines, { "NoClassDefFoundError: Could not initialize class" });
    if (!uninitialized.isEmpty()) {
        auto earlier = findLine(lines, { "java.lang.ExceptionInInitializerError" });
        findings.append({ tr("A mod failed while it was starting up"),
                          earlier.isEmpty() ? uninitialized : earlier,
                          tr("The line above is the mod that actually failed; the class-not-found line after it is the same mod "
                             "being asked for again. Scroll to the first ExceptionInInitializerError and take out the mod it "
                             "names, or update it to a build for this version of the game.") });
    }

    if (auto missing = findLine(lines, { "Missing or unsupported mandatory dependencies:", "Missing Mods:" }); !missing.isEmpty()) {
        findings.append({ tr("A mod is missing something it needs"),
                          missing,
                          tr("Install what this line names - usually a library mod that other mods sit on, or a loader new "
                             "enough for the mods installed. If it is already installed, the version is not the one being "
                             "asked for: match the number in this line rather than the name.") });
    }

    if (auto duplicates = findLine(lines, { "Found duplicate mods:", "Duplicate Mods:", "is present in multiple files" });
        !duplicates.isEmpty()) {
        findings.append({ tr("The same mod is installed twice"),
                          duplicates,
                          tr("Search the mods folder for each name listed and keep one file per mod, the newest. Two files with "
                             "different names can still hold the same mod, which is what happens when a mod bundles a library "
                             "that is also installed on its own.") });
    }

    if (auto incompatible = findLine(lines,
                                     { "Incompatible mods found!", "Mod resolution encountered an incompatible mod set!",
                                       "Some of your mods are incompatible with the game or each other!" });
        !incompatible.isEmpty()) {
        findings.append({ tr("The mods installed do not agree with each other"),
                          incompatible,
                          tr("Fabric writes under this line who wants what version, and often a suggested fix. Follow it: "
                             "change the version of the named mod, or remove it. Guessing which of the two is at fault wastes "
                             "an hour.") });
    }

    if (auto mixin = findLine(lines,
                              { "An unexpected critical error was encountered", "Critical injection failure:",
                                "Mixin apply failed" });
        !mixin.isEmpty()) {
        findings.append({ tr("A mod could not patch the game"),
                          mixin,
                          tr("A mixin is a mod rewriting the game's own code while it loads, and it fails when the code it "
                             "expects is not there. The line names the mod the mixin belongs to - replace it with the build for "
                             "this exact version of the game and of the loader.") });
    }

    if (auto window = findLine(lines,
                               { "GLFW error 65542", "The driver does not appear to support OpenGL", "Failed to create window:",
                                 "org.lwjgl.LWJGLException: Pixel format not accelerated" });
        !window.isEmpty()) {
        findings.append({ tr("The graphics driver cannot open a window"),
                          window,
                          tr("Update the graphics driver from Intel, AMD or Nvidia's own site rather than through Windows "
                             "Update. On a laptop with two GPUs, force the game onto the one that supports OpenGL; through "
                             "remote desktop there is no OpenGL at all and the game cannot start.") });
    }

    if (auto nativeLibrary = findLine(lines,
                                      { "[LWJGL] Failed to load a library. Possible solutions:", "Failed to locate library:",
                                        "Failed to extract" });
        !nativeLibrary.isEmpty()) {
        findings.append({ tr("A native library was not unpacked"),
                          nativeLibrary,
                          tr("The game's own dll and so files are unpacked into a temporary folder when it starts, and an "
                             "antivirus or a full disk stops that. Let this launcher and its instance folder through the "
                             "antivirus, then start again.") });
    }

    if (auto atlas = findLine(lines, { "Unable to fit: " }); !atlas.isEmpty()) {
        findings.append({ tr("There is no room left in the texture atlas"),
                          atlas,
                          tr("Everything the game draws from a texture sheet has to fit in one image. A high resolution "
                             "resource pack, or a mod adding many custom items, is what overflows it - lower the pack's "
                             "resolution or remove the mod that adds the most textures.") });
    }

    if (auto ids = findLine(lines, { "maximum id range exceeded" }); !ids.isEmpty()) {
        findings.append({ tr("The game ran out of registration numbers"),
                          ids,
                          tr("Versions before 1.13 have a hard limit on how many blocks and items can be registered, and a "
                             "large pack reaches it. Remove mods or move the pack to 1.13 or newer; there is no setting for "
                             "this.") });
    }

    if (auto jvm = findLine(lines, { "A fatal error has been detected by the Java Runtime Environment" }); !jvm.isEmpty()) {
        findings.append({ tr("The Java runtime itself crashed"),
                          jvm,
                          tr("This is not a mod error: the JVM wrote a file named hs_err_pid with the details. What matters is "
                             "which library the crash happened in - a graphics driver module there means update or roll back "
                             "the driver, and a mod's own dll means that mod is the one to remove.") });
    }

    // The vanilla report says what the game was busy with, and the deepest cause is the one that actually failed.
    auto report = indexOfLast(lines, QStringLiteral("---- Minecraft Crash Report ----"));
    if (report >= 0) {
        auto description = lineAt(lines, indexOfLast(lines, QStringLiteral("Description: "), report, report + 40));
        auto cause = lineAt(lines, indexOfLast(lines, QStringLiteral("Caused by: "), report, report + 400));
        QString detail = description.isEmpty() ? QStringLiteral("---- Minecraft Crash Report ----") : description;
        if (!cause.isEmpty() && cause != detail) {
            detail += QStringLiteral("\n") + cause;
        }
        findings.append({ tr("The game wrote a crash report"),
                          detail,
                          tr("The line above names what the game was doing when it stopped. Take out the mod it blames, or ask "
                             "its author - the whole report is smaller than one screenshot of the log.") });
    }

    return findings;
}
