// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Komorebi Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "MemoryAdvice.h"

#include <algorithm>

#include <QRegularExpression>
#include <QSet>

namespace {
constexpr int roundTo = 512;
constexpr int floorMiB = 1024;

auto roundDown(int value) -> int
{
    return std::max(roundTo, value / roundTo * roundTo);
}
}  // namespace

auto adviseMemory(long long totalRamMiB, int modCount) -> MemoryAdvice
{
    MemoryAdvice advice;

    advice.wantedMiB = 2048;
    if (modCount >= 100) {
        advice.wantedMiB = 8192;
    } else if (modCount >= 40) {
        advice.wantedMiB = 6144;
    } else if (modCount >= 10) {
        advice.wantedMiB = 4096;
    } else if (modCount >= 1) {
        advice.wantedMiB = 3072;
    }

    // Three quarters of the machine at most, and always something left over for the operating system, the
    // launcher itself and whatever else the player has open.
    auto share = static_cast<int>(totalRamMiB * 3 / 4);
    auto headroom = totalRamMiB > 3072 ? static_cast<int>(totalRamMiB - 2048) : static_cast<int>(totalRamMiB / 2);

    advice.maxMiB = advice.wantedMiB;
    if (share < advice.maxMiB) {
        advice.maxMiB = share;
        advice.limit = MemoryLimit::SystemShare;
    }
    if (headroom < advice.maxMiB) {
        advice.maxMiB = headroom;
        advice.limit = MemoryLimit::SystemHeadroom;
    }
    if (advice.maxMiB < floorMiB) {
        advice.maxMiB = floorMiB;
        advice.limit = MemoryLimit::Floor;
    }
    advice.maxMiB = roundDown(advice.maxMiB);

    // Growing the heap while the game runs is what the stutter after a few minutes in a new chunk is.
    advice.minMiB = advice.maxMiB;
    return advice;
}

auto suggestJvmArgs(int heapMiB) -> QStringList
{
    QStringList args;
    args << "-XX:+UseG1GC";

    // On Java 8 the default of this flag is "not set", which leaves the size of a collection to be worked out from
    // the heap instead of from a pause the game may take. Later versions ask for this much by themselves.
    args << "-XX:MaxGCPauseMillis=200";

    args << "-XX:+AlwaysPreTouch";
    args << "-XX:+PerfDisableSharedMem";

    // Direct buffers are handed back by System::gc, which under the default setting means a stop-the-world
    // collection of the whole heap; concurrent here means the same work without the pause.
    args << "-XX:+ExplicitGCInvokesConcurrent";

    args << "-XX:+ParallelRefProcEnabled";
    args << "-XX:+UseStringDeduplication";

    // Anything half a region wide is collected as humongous rather than with its generation, and the default
    // region is one two-thousandth of the heap, which the big arrays a modded pack builds run out of.
    if (heapMiB >= 6144) {
        args << "-XX:G1HeapRegionSize=32m";
    } else if (heapMiB >= 3072) {
        args << "-XX:G1HeapRegionSize=16m";
    }

    return args;
}

auto mergeJvmArgs(const QString& current, const QStringList& preset) -> QString
{
    QSet<QString> decided;
    for (const auto& arg : preset) {
        decided.insert(arg.section('=', 0, 0));
    }

    QStringList args;
    for (const auto& token : current.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
        if (!decided.contains(token.section('=', 0, 0))) {
            args.append(token);
        }
    }
    args.append(preset);

    return args.join(' ');
}
