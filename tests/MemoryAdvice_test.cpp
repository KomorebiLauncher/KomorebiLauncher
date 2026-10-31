// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QtTest>

#include "MemoryAdvice.h"

class MemoryAdviceTest : public QObject {
    Q_OBJECT

   private slots:
    /** A machine with room to spare gets what the game asks for and nothing else decides. */
    void test_WhenTheModsDecide()
    {
        QCOMPARE(adviseMemory(32768, 0).maxMiB, 2048);
        QCOMPARE(adviseMemory(32768, 5).maxMiB, 3072);
        QCOMPARE(adviseMemory(32768, 15).maxMiB, 4096);
        QCOMPARE(adviseMemory(32768, 50).maxMiB, 6144);
        QCOMPARE(adviseMemory(32768, 150).maxMiB, 8192);

        auto advice = adviseMemory(8192, 40);
        QCOMPARE(advice.maxMiB, 6144);
        QVERIFY(advice.limit == MemoryLimit::Mods);
    }

    /** A small machine cannot be asked to hand over more than it has. */
    void test_WhenTheSystemDecides()
    {
        auto headroom = adviseMemory(4096, 150);
        QCOMPARE(headroom.wantedMiB, 8192);
        QCOMPARE(headroom.maxMiB, 2048);
        QVERIFY(headroom.limit == MemoryLimit::SystemHeadroom);

        // Three quarters of 10 GiB is less than the pack asked for.
        auto share = adviseMemory(10240, 150);
        QCOMPARE(share.maxMiB, 7680);
        QVERIFY(share.limit == MemoryLimit::SystemShare);

        // On a 6 GiB machine the leftover matters before the share does.
        auto tight = adviseMemory(6144, 150);
        QCOMPARE(tight.maxMiB, 4096);
        QVERIFY(tight.limit == MemoryLimit::SystemHeadroom);

        // A 16 GiB machine can afford what the biggest pack asks for.
        auto plenty = adviseMemory(16384, 150);
        QCOMPARE(plenty.maxMiB, 8192);
        QVERIFY(plenty.limit == MemoryLimit::Mods);
    }

    /** Below a usable heap there is nothing left to do but say so and name the floor. */
    void test_WhenThereIsBarelyAnything()
    {
        auto advice = adviseMemory(1024, 0);
        QCOMPARE(advice.maxMiB, 1024);
        QVERIFY(advice.limit == MemoryLimit::Floor);
    }

    /** The two ends of the heap are the same number, so it never has to grow mid-game. */
    void test_MinEqualsMax()
    {
        for (auto total : { 2048LL, 4096LL, 8192LL, 16384LL, 65536LL }) {
            for (auto mods : { 0, 3, 12, 60, 200 }) {
                auto advice = adviseMemory(total, mods);
                QCOMPARE(advice.minMiB, advice.maxMiB);
                QVERIFY(advice.maxMiB > 0);
                QVERIFY(advice.maxMiB <= static_cast<int>(total));
            }
        }
    }

    /** The collector suggestions are memory arguments by another name, and those block the launch. */
    void test_NeverSuggestsAMemoryArgument()
    {
        for (auto heap : { 1024, 2048, 3072, 6144, 8192, 32768 }) {
            for (const auto& arg : suggestJvmArgs(heap)) {
                QVERIFY2(!arg.startsWith("-Xm"), qPrintable(arg));
                QVERIFY2(!arg.startsWith("-version:"), qPrintable(arg));
            }
        }
    }

    /** The region size is the one suggestion that depends on how big the heap is. */
    void test_RegionSizeFollowsTheHeap()
    {
        QVERIFY(!suggestJvmArgs(1024).contains("-XX:G1HeapRegionSize=16m"));
        QVERIFY(!suggestJvmArgs(2048).contains("-XX:G1HeapRegionSize=16m"));

        QVERIFY(suggestJvmArgs(3072).contains("-XX:G1HeapRegionSize=16m"));
        QVERIFY(!suggestJvmArgs(3072).contains("-XX:G1HeapRegionSize=32m"));

        QVERIFY(suggestJvmArgs(6144).contains("-XX:G1HeapRegionSize=32m"));
        QVERIFY(suggestJvmArgs(32768).contains("-XX:G1HeapRegionSize=32m"));
    }

    /** Everything else is a flag HotSpot accepts whether it is told to unlock experimentals or not. */
    void test_AllOfItIsAProductFlag()
    {
        for (const auto& arg : suggestJvmArgs(2048)) {
            QVERIFY2(arg.startsWith("-XX:"), qPrintable(arg));
        }
    }

    /** A player's own arguments survive, and the ones the suggestion decides are replaced rather than repeated. */
    void test_MergeKeepsWhatWasTyped()
    {
        auto preset = suggestJvmArgs(3072);
        auto merged = mergeJvmArgs("-XX:+UseG1GC -XX:MaxGCPauseMillis=50 -Dio.netty.tryReflectionSetAccessible=true", preset);

        auto tokens = merged.split(' ', Qt::SkipEmptyParts);
        QVERIFY(tokens.contains("-Dio.netty.tryReflectionSetAccessible=true"));
        QCOMPARE(tokens.count("-XX:MaxGCPauseMillis=200"), 1);
        QCOMPARE(tokens.count("-XX:MaxGCPauseMillis=50"), 0);
        QCOMPARE(tokens.count("-XX:+UseG1GC"), 1);
        QVERIFY(tokens.endsWith("-XX:G1HeapRegionSize=16m"));
    }

    /** Pressing the button twice has to be the same as pressing it once. */
    void test_MergeIsIdempotent()
    {
        auto once = mergeJvmArgs("-da", suggestJvmArgs(8192));
        auto twice = mergeJvmArgs(once, suggestJvmArgs(8192));
        QCOMPARE(twice, once);
    }
};

QTEST_GUILESS_MAIN(MemoryAdviceTest)

#include "MemoryAdvice_test.moc"
