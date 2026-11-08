// SPDX-License-Identifier: GPL-3.0-only
#include <QTest>

#include "komorebi/services/download/DownloadSource.h"

using namespace Komorebi::Services;

class KomorebiDownloadSourceTest : public QObject {
    Q_OBJECT

   private slots:
    void officialKeepsTheUrl();
    void mapsLibrariesOntoTheMirror();
    void mapsAssetsOntoTheMirror();
    void mapsMavenHostsOntoTheMirror();
    void keepsPistonPaths();
    void leavesHostsTheMirrorDoesNotServe();
    void readsTheStoredId();
};

void KomorebiDownloadSourceTest::officialKeepsTheUrl()
{
    const QUrl url(QStringLiteral("https://libraries.minecraft.net/com/example/a/1.0/a-1.0.jar"));
    QCOMPARE(sourceUrl(url, DownloadSource::Official), url);
}

void KomorebiDownloadSourceTest::mapsLibrariesOntoTheMirror()
{
    const auto mirrored = sourceUrl(QUrl(QStringLiteral("https://libraries.minecraft.net/com/example/a/1.0/a-1.0.jar")),
                                    DownloadSource::Bmclapi);
    QCOMPARE(mirrored.host(), QStringLiteral("bmclapi2.bangbang93.com"));
    QCOMPARE(mirrored.path(), QStringLiteral("/maven/com/example/a/1.0/a-1.0.jar"));
}

void KomorebiDownloadSourceTest::mapsAssetsOntoTheMirror()
{
    const auto mirrored = sourceUrl(QUrl(QStringLiteral("https://resources.download.minecraft.net/ab/abcdef0123")),
                                    DownloadSource::Bmclapi);
    QCOMPARE(mirrored.path(), QStringLiteral("/assets/ab/abcdef0123"));
}

void KomorebiDownloadSourceTest::mapsMavenHostsOntoTheMirror()
{
    for (const auto* host : { "maven.minecraftforge.net", "maven.fabricmc.net", "maven.quiltmc.org" }) {
        const auto mirrored =
            sourceUrl(QUrl(QStringLiteral("https://") + host + QStringLiteral("/org/example/lib/1.0/lib-1.0.jar")),
                      DownloadSource::Bmclapi);
        QCOMPARE(mirrored.host(), QStringLiteral("bmclapi2.bangbang93.com"));
        QCOMPARE(mirrored.path(), QStringLiteral("/maven/org/example/lib/1.0/lib-1.0.jar"));
    }
}

void KomorebiDownloadSourceTest::keepsPistonPaths()
{
    const auto mirrored =
        sourceUrl(QUrl(QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json")), DownloadSource::Bmclapi);
    QCOMPARE(mirrored.host(), QStringLiteral("bmclapi2.bangbang93.com"));
    QCOMPARE(mirrored.path(), QStringLiteral("/mc/game/version_manifest_v2.json"));
}

void KomorebiDownloadSourceTest::leavesHostsTheMirrorDoesNotServe()
{
    const QUrl url(QStringLiteral("https://files.prismlauncher.org/maven/io/github/zekerzhayard/ForgeWrapper.jar"));
    QCOMPARE(sourceUrl(url, DownloadSource::Bmclapi), url);
}

void KomorebiDownloadSourceTest::readsTheStoredId()
{
    QCOMPARE(downloadSourceFromId(QStringLiteral("bmclapi")), DownloadSource::Bmclapi);
    QCOMPARE(downloadSourceFromId(QStringLiteral("official")), DownloadSource::Official);
    QCOMPARE(downloadSourceFromId(QStringLiteral("nonsense")), DownloadSource::Official);
}

QTEST_GUILESS_MAIN(KomorebiDownloadSourceTest)

#include "KomorebiDownloadSource_test.moc"
