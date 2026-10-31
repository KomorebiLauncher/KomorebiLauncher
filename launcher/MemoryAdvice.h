// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Komorebi Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QStringList>

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
 * the free memory only moves the failure into the operating system.
 */
auto adviseMemory(long long totalRamMiB, int modCount) -> MemoryAdvice;

/**
 * Suggests the garbage collector arguments for a heap of heapMiB.
 *
 * Every flag here is a product flag of HotSpot from Java 8 to Java 25, so the JVM starts with it on any of them and
 * nothing has to unlock experimentals. They only take away the pauses that have nothing to do with the game: the
 * heap being touched page by page while the world is generated, statistics files being flushed to disk, and a full
 * garbage collection whenever something calls System::gc, which the network library does to hand back buffers.
 */
auto suggestJvmArgs(int heapMiB) -> QStringList;

/**
 * Adds preset to the arguments the player already typed.
 *
 * A flag given twice is decided by the last one, so anything the preset also sets is dropped from the old text
 * instead of being left behind as a contradiction.
 */
auto mergeJvmArgs(const QString& current, const QStringList& preset) -> QString;
