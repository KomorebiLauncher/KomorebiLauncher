// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
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
};

QTEST_GUILESS_MAIN(MemoryAdviceTest)

#include "MemoryAdvice_test.moc"
