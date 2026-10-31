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
                             QStringLiteral("    at java.base/java.util.Arrays.copyOf(Arrays.java:1511)") };
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

    /** Each rule fires on the wording the loader or the JVM really prints, behind a log4j prefix. */
    void test_Rules_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("title");

        QTest::newRow("missing dependency")
            << QStringLiteral("[14:28:38] [main/ERROR] [ne.mi.fm.lo.ModSorter/LOADING]: Missing or unsupported mandatory "
                              "dependencies:")
            << QStringLiteral("A mod is missing something it needs");
        QTest::newRow("duplicate mod")
            << QStringLiteral("[16:22:31] [main/ERROR] [fml.loading/]: Found duplicate mods:")
            << QStringLiteral("The same mod is installed twice");
        QTest::newRow("fabric conflict") << QStringLiteral("net.fabricmc.loader.impl.FormattedException: Incompatible mods found!")
                                         << QStringLiteral("The mods installed do not agree with each other");
        QTest::newRow("mixin")
            << QStringLiteral("org.spongepowered.asm.mixin.transformer.throwables.MixinTransformerError: An unexpected critical "
                              "error was encountered")
            << QStringLiteral("A mod could not patch the game");
        QTest::newRow("window") << QStringLiteral("[16:22:33] [Render thread/ERROR]: GLFW error 65542 : WGL: The driver does not "
                                                  "appear to support OpenGL")
                                << QStringLiteral("The graphics driver cannot open a window");
        QTest::newRow("native library") << QStringLiteral("[LWJGL] Failed to load a library. Possible solutions:")
                                        << QStringLiteral("A native library was not unpacked");
        QTest::newRow("atlas") << QStringLiteral("net.minecraft.client.texture.Stitcher$StitcherException: Unable to fit: "
                                                 "minecraft:textures/item/netherite_sword.png - size 512x512")
                               << QStringLiteral("There is no room left in the texture atlas");
        QTest::newRow("ids") << QStringLiteral("java.lang.IllegalStateException: maximum id range exceeded")
                             << QStringLiteral("The game ran out of registration numbers");
        QTest::newRow("jvm crash") << QStringLiteral("# A fatal error has been detected by the Java Runtime Environment:")
                                   << QStringLiteral("The Java runtime itself crashed");
        QTest::newRow("heap refused") << QStringLiteral("Error: Could not create the Java Virtual Machine.\nError: A fatal "
                                                       "exception has occurred. Error: Could not reserve enough space for object heap")
                                      << QStringLiteral("The memory requested cannot be given");
    }

    void test_Rules()
    {
        QFETCH(QString, line);
        QFETCH(QString, title);

        bool matched = false;
        for (const auto& finding : CrashAnalysis::analyze(line.split('\n'))) {
            matched = matched || finding.title == title;
        }
        QVERIFY2(matched, qPrintable(QStringLiteral("Expected '%1' from '%2'").arg(title, line)));
    }

    /** A class reported as missing was already reported failing to initialize, earlier in the log. */
    void test_PointsAtTheFirstFailure()
    {
        QStringList lines = { QStringLiteral("[12:00:00] [main/ERROR]: java.lang.ExceptionInInitializerError: cannot read config"),
                              QStringLiteral("[12:00:01] [main/ERROR]: java.lang.NoClassDefFoundError: Could not initialize class "
                                             "com.example.ModClass") };
        auto findings = CrashAnalysis::analyze(lines);
        QCOMPARE(findings.size(), 1);
        QCOMPARE(findings.at(0).title, QStringLiteral("A mod failed while it was starting up"));
        QVERIFY(findings.at(0).detail.contains(QStringLiteral("cannot read config")));
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
