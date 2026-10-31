// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

    void test_BuildsComponentFromRealInstallerOutput()
    {
        // Fixture captured from OptiFine's own installer run headlessly via OptiFineRunner.jar.
        auto path = QFINDTESTDATA("testdata/optifine-installer-output-1.13.2.json");
        QVERIFY2(!path.isEmpty(), "installer output fixture not found");
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto produced = QJsonDocument::fromJson(file.readAll()).object();
        QVERIFY(!produced.isEmpty());

        OptiFine::Build build{ QStringLiteral("1.13.2"), QStringLiteral("HD_U"), QStringLiteral("E7"),
                     QStringLiteral("OptiFine_1.13.2_HD_U_E7.jar"), QStringLiteral("Forge N/A"), true };
        QList<OptiFine::LocalLibrary> files{ OptiFine::LocalLibrary{ QStringLiteral("optifine/OptiFine/1.13.2_HD_U_E7/OptiFine-1.13.2_HD_U_E7.jar"),
                                                                    QStringLiteral("file:///C:/inst/libraries/optifine/OptiFine/1.13.2_HD_U_E7/OptiFine-1.13.2_HD_U_E7.jar"),
                                                                    QStringLiteral("0123456789abcdef0123456789abcdef01234567"), 2709891 } };
        auto component = OptiFine::buildComponent(produced, build, files);

        QCOMPARE(component.value("uid").toString(), QStringLiteral("komorebilauncher.optifine"));
        QCOMPARE(component.value("name").toString(), QStringLiteral("OptiFine HD_U_E7"));
        QCOMPARE(component.value("version").toString(), QStringLiteral("E7"));
        // What the launcher needs in order to load the profile alongside the right Minecraft.
        auto requirement = component.value("requires").toArray().at(0).toObject();
        QCOMPARE(requirement.value("uid").toString(), QStringLiteral("net.minecraft"));
        QCOMPARE(requirement.value("equals").toString(), QStringLiteral("1.13.2"));
        // The patched launch command must survive untouched.
        QCOMPARE(component.value("mainClass").toString(), QStringLiteral("net.minecraft.launchwrapper.Launch"));
        QCOMPARE(component.value("inheritsFrom").toString(), QStringLiteral("1.13.2"));
        QCOMPARE(component.value("arguments").toObject().value("game").toArray().at(1).toString(),
                 QStringLiteral("optifine.OptiFineTweaker"));

        auto libraries = component.value("libraries").toArray();
        QCOMPARE(libraries.size(), 2);
        auto optifine = libraries.at(0).toObject();
        QCOMPARE(optifine.value("name").toString(), QStringLiteral("optifine:OptiFine:1.13.2_HD_U_E7"));
        auto artifact = optifine.value("downloads").toObject().value("artifact").toObject();
        QCOMPARE(artifact.value("url").toString(),
                 QStringLiteral("file:///C:/inst/libraries/optifine/OptiFine/1.13.2_HD_U_E7/OptiFine-1.13.2_HD_U_E7.jar"));
        QCOMPARE(artifact.value("sha1").toString(), QStringLiteral("0123456789abcdef0123456789abcdef01234567"));
        QCOMPARE(artifact.value("size").toVariant().toLongLong(), 2709891LL);

        // A library that does have a maven home must be left exactly as the installer wrote it.
        QCOMPARE(libraries.at(1).toObject(), produced.value("libraries").toArray().at(1).toObject());
    }

    void test_BuildsComponentWhenInstallerEmittedNoOptiFineLibrary()
    {
        OptiFine::Build build{ QStringLiteral("1.13.2"), QStringLiteral("HD_U"), QStringLiteral("E7"), QString(), QString(), false };
        QJsonObject produced;
        produced.insert(QStringLiteral("libraries"), QJsonArray{ QJsonObject{ { "name", QStringLiteral("net.minecraft:launchwrapper:1.12") } } });

        auto component = OptiFine::buildComponent(produced, build, {});
        // Nothing to point at the local copy, but the component itself must still be well formed.
        QCOMPARE(component.value("uid").toString(), QStringLiteral("komorebilauncher.optifine"));
        QCOMPARE(component.value("libraries").toArray().size(), 1);
    }

    /** Newer OptiFine forks launchwrapper as well, and that artifact is published nowhere at all. */
    void test_BuildsComponentFromNewerInstallerOutput()
    {
        auto path = QFINDTESTDATA("testdata/optifine-installer-output-1.20.1.json");
        QVERIFY2(!path.isEmpty(), "1.20.1 installer output fixture not found");
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto produced = QJsonDocument::fromJson(file.readAll()).object();

        OptiFine::Build build{ QStringLiteral("1.20.1"), QStringLiteral("HD_U"), QStringLiteral("I6"),
                               QStringLiteral("OptiFine_1.20.1_HD_U_I6.jar"), QStringLiteral("Forge 47.2.18"), true };
        QList<OptiFine::LocalLibrary> files{
            OptiFine::LocalLibrary{ QStringLiteral("optifine/OptiFine/1.20.1_HD_U_I6/OptiFine-1.20.1_HD_U_I6.jar"),
                                    QStringLiteral("file:///inst/libraries/optifine/OptiFine/1.20.1_HD_U_I6/OptiFine-1.20.1_HD_U_I6.jar"),
                                    QStringLiteral("28458529e7153d0d6aefc81099141dc908a49005"), 6638739 },
            OptiFine::LocalLibrary{ QStringLiteral("optifine/launchwrapper-of/2.3/launchwrapper-of-2.3.jar"),
                                    QStringLiteral("file:///inst/libraries/optifine/launchwrapper-of/2.3/launchwrapper-of-2.3.jar"),
                                    QStringLiteral("073b25f1667904d481b10f706288c95ea9656f0c"), 15930 },
        };

        auto component = OptiFine::buildComponent(produced, build, files);
        auto libraries = component.value("libraries").toArray();
        QCOMPARE(libraries.size(), 2);
        for (const auto& value : libraries) {
            auto name = value.toObject().value("name").toString();
            auto artifact = value.toObject().value("downloads").toObject().value("artifact").toObject();
            QVERIFY2(!artifact.isEmpty(), qPrintable(name));
            QVERIFY2(artifact.value("url").toString().startsWith(QStringLiteral("file:///inst/libraries/")), qPrintable(name));
            QVERIFY(!artifact.value("sha1").toString().isEmpty());
            QVERIFY(artifact.value("size").toVariant().toLongLong() > 0);
        }
    }

    /** The path a maven name implies is how the files the installer left behind are recognised. */
    void test_MavenStoragePath()
    {
        QCOMPARE(OptiFine::mavenStoragePath(QStringLiteral("optifine:launchwrapper-of:2.3")),
                 QStringLiteral("optifine/launchwrapper-of/2.3/launchwrapper-of-2.3.jar"));
        QCOMPARE(OptiFine::mavenStoragePath(QStringLiteral("optifine:OptiFine:1.20.1_HD_U_I6")),
                 QStringLiteral("optifine/OptiFine/1.20.1_HD_U_I6/OptiFine-1.20.1_HD_U_I6.jar"));
        QCOMPARE(OptiFine::mavenStoragePath(QStringLiteral("com.mojang:patchy:1.3.9")),
                 QStringLiteral("com/mojang/patchy/1.3.9/patchy-1.3.9.jar"));
        QVERIFY(OptiFine::mavenStoragePath(QStringLiteral("not-a-coordinate")).isEmpty());
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
