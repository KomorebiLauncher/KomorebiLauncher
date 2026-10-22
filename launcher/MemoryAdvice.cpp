// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "MemoryAdvice.h"

#include <algorithm>

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
