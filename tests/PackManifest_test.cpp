// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QTest>

#include "modplatform/domestic/PackManifest.h"

using namespace Domestic;

namespace {

auto toBytes(const char* text) -> QByteArray
{
    return QByteArray(text);
}

}  // namespace

class PackManifestTest : public QObject {
    Q_OBJECT

   private slots:
    void detectsMcbbsByPackMeta();
    void detectsHmclByBothFiles();
    void ignoresArchivesWithNeither();
    void detectsPackMetaAnywhereInTheTree();

    void parsesMcbbsRootGameVersion();
    void parsesMcbbsVersionUnderLaunch();
    void parsesMcbbsVersionUnderLaunchInfo();
    void rejectsMcbbsWithoutGameVersion();
    void readsMcbbsCurseforgeFiles();
    void readsMcbbsDirectUrlFiles();
    void skipsMcbbsEntriesWithNoSource();

    void parsesHmclNameAndFabricLoader();
    void parsesHmclForgeVersionWithoutGamePrefix();
    void survivesBrokenHmclInstanceManifest();

    void infersLoaderFromMavenCoordinates();
    void ignoresUnknownLibraries();
};

void PackManifestTest::detectsMcbbsByPackMeta()
{
    QCOMPARE(detectFormat({ "mcbbs.packmeta", "overrides/mods/foo.jar" }), PackFormat::Mcbbs);
}

void PackManifestTest::detectsHmclByBothFiles()
{
    QCOMPARE(detectFormat({ "modpack.json", "minecraft/pack.json" }), PackFormat::HMCL);
}

void PackManifestTest::ignoresArchivesWithNeither()
{
    QCOMPARE(detectFormat({ "manifest.json", "overrides/foo.jar" }), PackFormat::Unknown);
    // modpack.json alone is not enough: it is also the name HMCL gives to a file inside some
    // packs, so requiring the instance manifest is what keeps this from misfiring.
    QCOMPARE(detectFormat({ "modpack.json" }), PackFormat::Unknown);
}

void PackManifestTest::detectsPackMetaAnywhereInTheTree()
{
    QCOMPARE(detectFormat({ "some/nested/mcbbs.packmeta" }), PackFormat::Mcbbs);
}

void PackManifestTest::parsesMcbbsRootGameVersion()
{
    PackDescription out;
    QString error;
    const auto ok = parseMcbbs(toBytes(R"({
        "manifestType": "minecraftModpack",
        "name": "Test Pack",
        "version": "1.0",
        "author": "someone",
        "gameVersion": "1.20.1",
        "loader": "fabric",
        "loaderVersion": "0.15.11"
    })"),
                               out, &error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(out.format, PackFormat::Mcbbs);
    QCOMPARE(out.name, QString("Test Pack"));
    QCOMPARE(out.gameVersion, QString("1.20.1"));
    QCOMPARE(out.loader, QString("fabric"));
    QCOMPARE(out.loaderVersion, QString("0.15.11"));
    QCOMPARE(out.overridesPrefix, QString("overrides/"));
}

void PackManifestTest::parsesMcbbsVersionUnderLaunch()
{
    PackDescription out;
    QString error;
    const auto ok = parseMcbbs(toBytes(R"({
        "name": "Old Pack",
        "launch": { "gameVersion": "1.12.2", "loader": "forge", "loaderVersion": "14.23.5.2860" }
    })"),
                               out, &error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(out.gameVersion, QString("1.12.2"));
    QCOMPARE(out.loader, QString("forge"));
    QCOMPARE(out.loaderVersion, QString("14.23.5.2860"));
}

void PackManifestTest::parsesMcbbsVersionUnderLaunchInfo()
{
    PackDescription out;
    QString error;
    const auto ok = parseMcbbs(toBytes(R"({
        "name": "Newer Pack",
        "launchInfo": { "gameVersion": "1.19.2", "loaderType": "NeoForge", "loaderVersion": "20.4.237" }
    })"),
                               out, &error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(out.gameVersion, QString("1.19.2"));
    QCOMPARE(out.loader, QString("neoforge"));
    QCOMPARE(out.loaderVersion, QString("20.4.237"));
}

void PackManifestTest::rejectsMcbbsWithoutGameVersion()
{
    PackDescription out;
    QString error;
    QVERIFY(!parseMcbbs(toBytes(R"({ "name": "No Version" })"), out, &error));
    QVERIFY(!error.isEmpty());
}

void PackManifestTest::readsMcbbsCurseforgeFiles()
{
    PackDescription out;
    QString error;
    const auto ok = parseMcbbs(toBytes(R"({
        "gameVersion": "1.16.5",
        "files": [ { "path": "mods/jei.jar", "projectID": 238222, "fileID": 3000001, "required": true } ]
    })"),
                               out, &error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(out.files.size(), 1);
    QCOMPARE(out.files.at(0).path, QString("mods/jei.jar"));
    QCOMPARE(out.files.at(0).curseforgeProjectId, 238222);
    QCOMPARE(out.files.at(0).curseforgeFileId, 3000001);
}

void PackManifestTest::readsMcbbsDirectUrlFiles()
{
    PackDescription out;
    QString error;
    const auto ok = parseMcbbs(toBytes(R"({
        "gameVersion": "1.18.2",
        "files": [ {
            "path": "resourcepacks/pack.zip",
            "url": "https://example.invalid/pack.zip",
            "hashes": { "sha1": "aa11" },
            "size": 42
        } ]
    })"),
                               out, &error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(out.files.size(), 1);
    QCOMPARE(out.files.at(0).url, QString("https://example.invalid/pack.zip"));
    QCOMPARE(out.files.at(0).sha1, QString("aa11"));
    QCOMPARE(out.files.at(0).size, qint64(42));
}

void PackManifestTest::skipsMcbbsEntriesWithNoSource()
{
    PackDescription out;
    QString error;
    const auto ok = parseMcbbs(toBytes(R"({
        "gameVersion": "1.16.5",
        "files": [ { "path": "config/bundled.cfg" }, { "path": "" } ]
    })"),
                               out, &error);

    QVERIFY2(ok, qPrintable(error));
    QVERIFY(out.files.isEmpty());
    QCOMPARE(out.warnings.size(), 1);
}

void PackManifestTest::parsesHmclNameAndFabricLoader()
{
    PackDescription out;
    QString error;
    const auto ok = parseHmcl(
        toBytes(R"({ "name": "HMCL Pack", "version": "2.0", "author": "someone" })"),
        toBytes(R"({
            "jar": "1.20.4",
            "libraries": [ { "name": "net.fabricmc:fabric-loader:0.15.11" } ]
        })"),
        out, &error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(out.format, PackFormat::HMCL);
    QCOMPARE(out.name, QString("HMCL Pack"));
    QCOMPARE(out.gameVersion, QString("1.20.4"));
    QCOMPARE(out.loader, QString("fabric"));
    QCOMPARE(out.loaderVersion, QString("0.15.11"));
    QCOMPARE(out.overridesPrefix, QString("minecraft/"));
}

void PackManifestTest::parsesHmclForgeVersionWithoutGamePrefix()
{
    PackDescription out;
    QString error;
    const auto ok = parseHmcl(
        toBytes(R"({ "name": "Forge Pack" })"),
        toBytes(R"({ "jar": "1.16.5", "libraries": [ { "name": "net.minecraftforge:forge:1.16.5-36.2.34" } ] })"), out, &error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(out.loader, QString("forge"));
    QCOMPARE(out.loaderVersion, QString("36.2.34"));
}

void PackManifestTest::survivesBrokenHmclInstanceManifest()
{
    PackDescription out;
    QString error;
    const auto ok = parseHmcl(toBytes(R"({ "name": "Broken" })"), toBytes("{ not json"), out, &error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(out.name, QString("Broken"));
    QVERIFY(out.gameVersion.isEmpty());
    QCOMPARE(out.warnings.size(), 1);
}

void PackManifestTest::infersLoaderFromMavenCoordinates()
{
    QString loader;
    QString version;

    QVERIFY(loaderFromLibraries({ "org.quiltmc:quilt-loader:0.23.1" }, loader, version));
    QCOMPARE(loader, QString("quilt"));
    QCOMPARE(version, QString("0.23.1"));

    QVERIFY(loaderFromLibraries({ "net.neoforged:neoforge:20.4.237" }, loader, version));
    QCOMPARE(loader, QString("neoforge"));
    QCOMPARE(version, QString("20.4.237"));
}

void PackManifestTest::ignoresUnknownLibraries()
{
    QString loader;
    QString version;
    QVERIFY(!loaderFromLibraries({ "com.google.guava:guava:32.1.2-jre", "malformed" }, loader, version));
    QVERIFY(loader.isEmpty());
}

QTEST_GUILESS_MAIN(PackManifestTest)

#include "PackManifest_test.moc"
