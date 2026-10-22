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

namespace {

/** The last line carrying the text, which is the one nearest the point of failure. */
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

    auto heap = lineAt(lines, indexOfLast(lines, QStringLiteral("java.lang.OutOfMemoryError: Java heap space")));
    auto metaspace = lineAt(lines, indexOfLast(lines, QStringLiteral("java.lang.OutOfMemoryError: Metaspace")));
    auto anyOom = lineAt(lines, indexOfLast(lines, QStringLiteral("java.lang.OutOfMemoryError")));

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

    auto classVersion = lineAt(lines, indexOfLast(lines, QStringLiteral("java.lang.UnsupportedClassVersionError")));
    if (!classVersion.isEmpty()) {
        auto wanted = javaReleaseForClassVersion(classVersionIn(classVersion, QStringLiteral("class file version")));
        auto present = javaReleaseForClassVersion(classVersionIn(classVersion, QStringLiteral("versions up to")));
        auto advice = tr("This instance is launching with Java %1, but something in it was compiled for Java %2. Set a newer "
                         "Java in this instance's Java settings, or remove the mod that needs it.")
                          .arg(present.isEmpty() ? tr("an older version") : present,
                               wanted.isEmpty() ? tr("a newer version") : wanted);
        findings.append({ tr("A mod needs a newer Java than the one in use"), classVersion, advice });
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
