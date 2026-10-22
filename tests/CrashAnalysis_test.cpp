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

#include "minecraft/CrashAnalysis.h"

class CrashAnalysisTest : public QObject {
    Q_OBJECT

   private slots:
    void test_ClassFileVersionTable()
    {
        QCOMPARE(CrashAnalysis::javaReleaseForClassVersion(49), QStringLiteral("1.5"));
        QCOMPARE(CrashAnalysis::javaReleaseForClassVersion(52), QStringLiteral("1.8"));
        QCOMPARE(CrashAnalysis::javaReleaseForClassVersion(55), QStringLiteral("11"));
        QCOMPARE(CrashAnalysis::javaReleaseForClassVersion(61), QStringLiteral("17"));
        QCOMPARE(CrashAnalysis::javaReleaseForClassVersion(65), QStringLiteral("21"));

        // Outside the table, better to say nothing than to name the wrong Java.
        QVERIFY(CrashAnalysis::javaReleaseForClassVersion(44).isEmpty());
        QVERIFY(CrashAnalysis::javaReleaseForClassVersion(0).isEmpty());
    }

    void test_OutOfMemory()
    {
        QStringList heap = { QStringLiteral("[12:00:01] [Render thread/ERROR]: java.lang.OutOfMemoryError: Java heap space"),
                             QStringLiteral("[12:00:01] [Render thread/ERROR]:     at java.base/java.util.Arrays.copyOf(Arrays.java:1511)") };
        auto findings = CrashAnalysis::analyze(heap);
        QCOMPARE(findings.size(), 1);
        QCOMPARE(findings.at(0).title, QStringLiteral("The game ran out of memory"));
        QVERIFY(findings.at(0).suggestion.contains(QStringLiteral("memory")));

        QStringList metaspace = { QStringLiteral("[12:00:01] [main/ERROR]: java.lang.OutOfMemoryError: Metaspace") };
        findings = CrashAnalysis::analyze(metaspace);
        QCOMPARE(findings.size(), 1);
        QCOMPARE(findings.at(0).title, QStringLiteral("The game ran out of class space"));
        QVERIFY(findings.at(0).suggestion.contains(QStringLiteral("MaxMetaspaceSize")));

        // Some other kind of exhaustion still gets named, once.
        QStringList native = { QStringLiteral("java.lang.OutOfMemoryError: unable to make native thread") };
        findings = CrashAnalysis::analyze(native);
        QCOMPARE(findings.size(), 1);
        QCOMPARE(findings.at(0).title, QStringLiteral("The game ran out of memory"));
    }

    void test_WrongJava()
    {
        QStringList lines = { QStringLiteral(
            "java.lang.UnsupportedClassVersionError: org/jeijei/jei/JeiInit has been compiled by a more recent version of the Java "
            "Runtime (class file version 65.0), this version of the Java Runtime only recognizes class file versions up to 61.0") };
        auto findings = CrashAnalysis::analyze(lines);
        QCOMPARE(findings.size(), 1);
        QCOMPARE(findings.at(0).title, QStringLiteral("A mod needs a newer Java than the one in use"));
        QVERIFY(findings.at(0).suggestion.contains(QStringLiteral("Java 21")));
        QVERIFY(findings.at(0).suggestion.contains(QStringLiteral("Java 17")));
    }

    void test_CrashReport()
    {
        QStringList lines = { QStringLiteral("some earlier noise"),
                              QStringLiteral("---- Minecraft Crash Report ----"),
                              QStringLiteral("// Don't be sad, have a hug! <3"),
                              QStringLiteral("Description: Rendering overlay"),
                              QStringLiteral("Caused by: java.lang.RuntimeException: the first thing blamed"),
                              QStringLiteral("Caused by: java.io.IOException: the real reason"),
                              QStringLiteral("Memory: 123456 bytes") };
        auto findings = CrashAnalysis::analyze(lines);
        QCOMPARE(findings.size(), 1);
        QVERIFY(findings.at(0).detail.contains(QStringLiteral("Description: Rendering overlay")));

        // The deepest cause is the one that actually failed.
        QVERIFY(findings.at(0).detail.contains(QStringLiteral("the real reason")));
        QVERIFY(!findings.at(0).detail.contains(QStringLiteral("the first thing blamed")));
    }

    /** A diagnosis nobody asked for is a false alarm; nothing here must fire on an ordinary log. */
    void test_SaysNothingItCannotBackUp()
    {
        QStringList ordinary = { QStringLiteral("[12:00:00] [main/INFO]: Mod Launcher launched with 200 mods"),
                                 QStringLiteral("[12:00:01] [Render thread/INFO]: OpenAL initialized on device OpenAL Soft"),
                                 QStringLiteral("Memory: 4096M, Heap: 1024M"),
                                 QStringLiteral("[12:00:02] [Server thread/INFO]: Preparing start region") };
        QVERIFY(CrashAnalysis::analyze(ordinary).isEmpty());
        QVERIFY(CrashAnalysis::analyze({}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(CrashAnalysisTest)

#include "CrashAnalysis_test.moc"
