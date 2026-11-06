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

#include "net/AdoptiumMirror.h"
#include "net/MirrorPolicy.h"

namespace {

const auto kGithubJdk21 =
    QStringLiteral("https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.13%2B9/"
                   "OpenJDK21U-jdk_x64_windows_hotspot_21.0.13_9.zip");

}  // namespace

class AdoptiumMirrorTest : public QObject {
    Q_OBJECT

   private slots:
    void translatesGithubReleaseAsset();
    void translatesOtherPlatforms();
    void translatesTarGz();
    void keepsQueryOutOfThePath();

    void ignoresUrlsWithoutAnAssetName();
    void ignoresNonAdoptiumHosts();
    void ignoresEmptyBase();
    void recognisesAdoptiumDownloads();

    void policyOffersTheMirrorEvenWithoutAMojangBase();
};

void AdoptiumMirrorTest::translatesGithubReleaseAsset()
{
    const auto mirrored = Net::adoptiumMirrorUrl(QUrl(kGithubJdk21));
    QVERIFY(mirrored.isValid());
    QCOMPARE(mirrored.toString(),
             QStringLiteral("https://mirrors.tuna.tsinghua.edu.cn/Adoptium/21/jdk/x64/windows/"
                            "OpenJDK21U-jdk_x64_windows_hotspot_21.0.13_9.zip"));
}

void AdoptiumMirrorTest::translatesOtherPlatforms()
{
    const auto mirrored =
        Net::adoptiumMirrorUrl(QUrl("https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.20%2B1/"
                                    "OpenJDK17U-jre_aarch64_linux_hotspot_17.0.20.1_1.tar.gz"));
    QVERIFY(mirrored.isValid());
    QCOMPARE(mirrored.path(), QStringLiteral("/Adoptium/17/jre/aarch64/linux/OpenJDK17U-jre_aarch64_linux_hotspot_17.0.20.1_1.tar.gz"));
}

void AdoptiumMirrorTest::translatesTarGz()
{
    const auto mirrored =
        Net::adoptiumMirrorUrl(QUrl("https://github.com/adoptium/temurin8-binaries/releases/download/jdk8u504-b01/"
                                    "OpenJDK8U-jdk_x64_linux_hotspot_8u504b01.tar.gz"));
    QVERIFY(mirrored.isValid());
    QCOMPARE(mirrored.path(), QStringLiteral("/Adoptium/8/jdk/x64/linux/OpenJDK8U-jdk_x64_linux_hotspot_8u504b01.tar.gz"));
}

void AdoptiumMirrorTest::keepsQueryOutOfThePath()
{
    QUrl official(kGithubJdk21);
    official.setQuery(QStringLiteral("download=1"));
    const auto mirrored = Net::adoptiumMirrorUrl(official);
    QVERIFY(mirrored.isValid());
    QVERIFY(mirrored.query().isEmpty());
}

void AdoptiumMirrorTest::ignoresUrlsWithoutAnAssetName()
{
    // This is the shape the Adoptium API uses when asked for "the latest build": the response
    // redirects to a file whose name is only known after asking, so no mirror path can be built.
    QVERIFY(!Net::adoptiumMirrorUrl(QUrl("https://api.adoptium.net/v3/binary/latest/21/ga/windows/x64/jdk/hotspot/normal/eclipse"))
                 .isValid());

    // A GitHub release page rather than an asset.
    QVERIFY(!Net::adoptiumMirrorUrl(QUrl("https://github.com/adoptium/temurin21-binaries/releases/latest")).isValid());

    // An asset name that is not one of the archive shapes the mirror carries.
    QVERIFY(!Net::adoptiumMirrorUrl(QUrl("https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.13%2B9/"
                                         "OpenJDK21U-jdk_x64_windows_hotspot_21.0.13_9.msi"))
                 .isValid());
}

void AdoptiumMirrorTest::ignoresNonAdoptiumHosts()
{
    QVERIFY(!Net::adoptiumMirrorUrl(QUrl("https://launchermeta.mojang.com/v1/products/java-runtime/abc/all.json")).isValid());
    QVERIFY(!Net::adoptiumMirrorUrl(QUrl("https://github.com/other/project/releases/download/v1/OpenJDK21U-jdk_x64_windows_hotspot_21.0.13_9.zip"))
                 .isValid());
}

void AdoptiumMirrorTest::ignoresEmptyBase()
{
    QVERIFY(!Net::adoptiumMirrorUrl(QUrl(kGithubJdk21), QString()).isValid());
}

void AdoptiumMirrorTest::recognisesAdoptiumDownloads()
{
    QVERIFY(Net::looksLikeAdoptiumDownload(QUrl(kGithubJdk21)));
    QVERIFY(Net::looksLikeAdoptiumDownload(QUrl("https://api.adoptium.net/v3/binary/latest/21/ga/windows/x64/jdk/hotspot/normal/eclipse")));
    QVERIFY(!Net::looksLikeAdoptiumDownload(QUrl("https://piston-data.mojang.com/v1/objects/abc/jvm.dll")));
}

void AdoptiumMirrorTest::policyOffersTheMirrorEvenWithoutAMojangBase()
{
    // No Mojang mirror configured, which is the default state of a fresh install.
    const auto candidates = Net::MirrorPolicy::candidatesFor(QUrl(kGithubJdk21), Net::MirrorPolicyKind::OfficialThenMirror, {},
                                                             {}, {});
    QCOMPARE(candidates.size(), size_t(2));
    QCOMPARE(candidates.at(0).toString(), kGithubJdk21);
    QVERIFY(candidates.at(1).toString().startsWith(QStringLiteral("https://mirrors.tuna.tsinghua.edu.cn/Adoptium/21/jdk/x64/windows/")));

    const auto mirroredFirst =
        Net::MirrorPolicy::candidatesFor(QUrl(kGithubJdk21), Net::MirrorPolicyKind::MirrorOnly, {}, {}, {});
    QCOMPARE(mirroredFirst.size(), size_t(1));
    QVERIFY(mirroredFirst.at(0).toString().contains(QStringLiteral("mirrors.tuna.tsinghua.edu.cn")));
}

QTEST_GUILESS_MAIN(AdoptiumMirrorTest)

#include "AdoptiumMirror_test.moc"
