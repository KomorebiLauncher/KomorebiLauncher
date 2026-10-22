// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

/** Which rule decided the heap size, so the interface can say why. */
enum class MemoryLimit { Mods, SystemShare, SystemHeadroom, Floor };

/** What the heap of an instance ought to be, worked out from the machine and what is installed. */
struct MemoryAdvice {
    int minMiB = 0;
    int maxMiB = 0;
    int wantedMiB = 0;
    MemoryLimit limit = MemoryLimit::Mods;
};

/**
 * Suggests a heap for a machine with totalRamMiB of memory running an instance with modCount mods.
 *
 * The ceiling is what the rest of the system needs, not what the game can be made to ask for: a heap larger than
 * the free memory only moves the failure into the operating system. The collector is left to the JVM, whose
 * default has been the right answer for a client since Java 9.
 */
auto adviseMemory(long long totalRamMiB, int modCount) -> MemoryAdvice;
