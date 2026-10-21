// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QTest>

#include "modplatform/optifine/OptiFineList.h"

class OptiFineListTest : public QObject {
    Q_OBJECT

    // Field shapes copied from the live version list: type carries the edition letters for newer
    // builds (HD_U_K1) and not for older ones (HD_U), and previews put pre1 in patch.
    static auto fixture() -> QByteArray
    {
        return R"JSON([
  {"mcversion":"1.13.2","type":"HD_U","patch":"E7","filename":"OptiFine_1.13.2_HD_U_E7.jar","forge":"Forge N/A"},
  {"mcversion":"1.8.9","type":"HD_U","patch":"M5","filename":"OptiFine_1.8.9_HD_U_M5.jar","forge":"Forge #1902"},
  {"mcversion":"26.1.2","type":"HD_U_K1","patch":"pre1","filename":"preview_OptiFine_26.1.2_HD_U_K1_pre1.jar","forge":"Forge N/A"},
  {"mcversion":"1.12.2","type":"HD_U_F6","patch":"pre1","filename":"preview_OptiFine_1.12.2_HD_U_F6_pre1.jar"},
  {"mcversion":"1.7.10","type":"HD_U","filename":"broken.jar"}
])JSON";
    }

   private slots:
    void test_ParsesAndDropsIncomplete()
    {
        auto builds = OptiFine::parseBuilds(fixture());
        QCOMPARE(builds.size(), 4);  // the entry without a patch is dropped
        QCOMPARE(builds.at(0).mcVersion, QStringLiteral("1.13.2"));
        QCOMPARE(builds.at(1).hasForgeNote, true);
        QCOMPARE(builds.at(1).forgeNote, QStringLiteral("Forge #1902"));
        QCOMPARE(builds.at(3).hasForgeNote, false);  // the field is absent, not empty
    }

    void test_BadInputYieldsNothing()
    {
        QVERIFY(OptiFine::parseBuilds(QByteArray()).isEmpty());
        QVERIFY(OptiFine::parseBuilds("not json").isEmpty());
        QVERIFY(OptiFine::parseBuilds(R"({"mcversion":"1.0"})").isEmpty());  // object, not array
    }

    void test_Identifiers()
    {
        auto builds = OptiFine::parseBuilds(fixture());

        auto old = builds.at(0);
        QCOMPARE(OptiFine::versionId(old), QStringLiteral("1.13.2-OptiFine_HD_U_E7"));
        QCOMPARE(OptiFine::mavenVersion(old), QStringLiteral("1.13.2_HD_U_E7"));

        // A preview build folds its edition into type, and the identifiers must not assume otherwise.
        auto preview = builds.at(2);
        QCOMPARE(OptiFine::versionId(preview), QStringLiteral("26.1.2-OptiFine_HD_U_K1_pre1"));
        QCOMPARE(OptiFine::mavenVersion(preview), QStringLiteral("26.1.2_HD_U_K1_pre1"));
    }

    void test_DownloadUrl()
    {
        auto build = OptiFine::parseBuilds(fixture()).at(1);
        auto expected = QStringLiteral("https://mirror.example/optifine/1.8.9/HD_U/M5");

        QCOMPARE(OptiFine::downloadUrl(QStringLiteral("https://mirror.example"), build).toString(), expected);
        QCOMPARE(OptiFine::downloadUrl(QStringLiteral("https://mirror.example///"), build).toString(), expected);
    }

    void test_BuildsForFiltersByVersion()
    {
        auto builds = OptiFine::parseBuilds(fixture());
        QCOMPARE(OptiFine::buildsFor(builds, QStringLiteral("1.13.2")).size(), 1);
        QCOMPARE(OptiFine::buildsFor(builds, QStringLiteral("1.99.99")).size(), 0);
    }
};

QTEST_GUILESS_MAIN(OptiFineListTest)

#include "OptiFineList_test.moc"
