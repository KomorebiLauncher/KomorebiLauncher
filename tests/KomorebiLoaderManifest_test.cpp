// SPDX-License-Identifier: GPL-3.0-only
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "komorebi/core/version/LoaderManifest.h"
#include "komorebi/core/version/VersionJson.h"

using namespace Komorebi::Core;

namespace {

QJsonArray fabricLoaderList()
{
    return QJsonArray{
        QJsonObject{ { QStringLiteral("loader"),
                       QJsonObject{ { QStringLiteral("version"), QStringLiteral("0.15.11") },
                                    { QStringLiteral("stable"), true } } } },
        QJsonObject{ { QStringLiteral("loader"),
                       QJsonObject{ { QStringLiteral("version"), QStringLiteral("0.16.0-beta.1") },
                                    { QStringLiteral("stable"), false } } } },
    };
}

QJsonObject fabricProfile()
{
    const QJsonObject artifact{ { QStringLiteral("name"), QStringLiteral("org.ow2.asm:asm:9.10.1") },
                                { QStringLiteral("url"), QStringLiteral("https://maven.fabricmc.net/") },
                                { QStringLiteral("sha1"), QStringLiteral("ada2141c0cc52ee8f5c48cd5fa4ce0e794f22236") },
                                { QStringLiteral("size"), 126151 } };
    return QJsonObject{
        { QStringLiteral("id"), QStringLiteral("fabric-loader-0.15.11-1.20.1") },
        { QStringLiteral("mainClass"), QStringLiteral("net.fabricmc.loader.impl.launch.knot.KnotClient") },
        { QStringLiteral("libraries"), QJsonArray{ artifact } },
        { QStringLiteral("arguments"),
          QJsonObject{ { QStringLiteral("game"), QJsonArray{} },
                       { QStringLiteral("jvm"), QJsonArray{ QStringLiteral("-DFabricMcEmu= net.minecraft.client.main.Main ") } } } },
    };
}

}  // namespace

class KomorebiLoaderManifestTest : public QObject {
    Q_OBJECT

   private slots:
    void readsTheLoaderList();
    void readsTheQuiltLoaderList();
    void convertsAProfileIntoComponentMetadata();
    void refusesAProfileWithoutAMainClass();
    void convertsAnInstalledForgeVersion();
};

void KomorebiLoaderManifestTest::readsTheLoaderList()
{
    QString error;
    const auto releases = parseFabricLoaderList(fabricLoaderList(), &error);
    QVERIFY2(releases.has_value(), qPrintable(error));
    QCOMPARE(releases->size(), 2);
    QCOMPARE(releases->first().version, QStringLiteral("0.15.11"));
    QVERIFY(releases->first().stable);
    QVERIFY(!releases->at(1).stable);
}

void KomorebiLoaderManifestTest::readsTheQuiltLoaderList()
{
    const QJsonArray document{
        QJsonObject{ { QStringLiteral("loader"),
                       QJsonObject{ { QStringLiteral("version"), QStringLiteral("0.20.0") } } } },
        QJsonObject{ { QStringLiteral("loader"),
                       QJsonObject{ { QStringLiteral("version"), QStringLiteral("0.20.0-beta.9") } } } },
    };
    QString error;
    const auto releases = parseQuiltLoaderList(document, &error);
    QVERIFY2(releases.has_value(), qPrintable(error));
    QCOMPARE(releases->size(), 2);
    QVERIFY(releases->first().stable);
    QVERIFY(!releases->at(1).stable);
}
void KomorebiLoaderManifestTest::convertsAProfileIntoComponentMetadata()
{
    QString error;
    const auto component = componentFromLoaderProfile(fabricProfile(), QStringLiteral("net.fabricmc.fabric-loader"), QStringLiteral("Fabric Loader"),
                                                 QStringLiteral("0.15.11"), &error);
    QVERIFY2(component.has_value(), qPrintable(error));

    QCOMPARE(component->value(QStringLiteral("uid")).toString(), QStringLiteral("net.fabricmc.fabric-loader"));
    QCOMPARE(component->value(QStringLiteral("version")).toString(), QStringLiteral("0.15.11"));
    QCOMPARE(component->value(QStringLiteral("mainClass")).toString(),
             QStringLiteral("net.fabricmc.loader.impl.launch.knot.KnotClient"));

    const auto libraries = component->value(QStringLiteral("libraries")).toArray();
    QCOMPARE(libraries.size(), 1);
    const auto artifact = libraries.first().toObject().value(QStringLiteral("downloads")).toObject().value(
        QStringLiteral("artifact"));
    QCOMPARE(artifact.toObject().value(QStringLiteral("url")).toString(),
             QStringLiteral("https://maven.fabricmc.net/org/ow2/asm/asm/9.10.1/asm-9.10.1.jar"));

    // What the launcher's own parser sees has to be the same library and the same JVM argument.
    const auto manifest = VersionJson::parse(*component, &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));
    QCOMPARE(manifest->mainClass, QStringLiteral("net.fabricmc.loader.impl.launch.knot.KnotClient"));
    QCOMPARE(manifest->libraries.size(), 1);
    QVERIFY(manifest->jvmArguments.contains(QStringLiteral("-DFabricMcEmu= net.minecraft.client.main.Main ")));
}

void KomorebiLoaderManifestTest::convertsAnInstalledForgeVersion()
{
    const QJsonObject document{
        { QStringLiteral("id"), QStringLiteral("1.20.1-forge-47.4.0") },
        { QStringLiteral("inheritsFrom"), QStringLiteral("1.20.1") },
        { QStringLiteral("mainClass"), QStringLiteral("cpw.mods.bootstraplauncher.BootstrapLauncher") },
        { QStringLiteral("libraries"),
          QJsonArray{ QJsonObject{ { QStringLiteral("name"), QStringLiteral("cpw.mods:securejarhandler:2.1.10") } } } },
        { QStringLiteral("arguments"),
          QJsonObject{
              { QStringLiteral("game"),
                QJsonArray{ QStringLiteral("--launchTarget"), QStringLiteral("forgeclient"),
                            QStringLiteral("--fml.forgeVersion"), QStringLiteral("47.4.0") } },
              { QStringLiteral("jvm"),
                QJsonArray{ QStringLiteral("-DlibraryDirectory=${library_directory}"),
                            QStringLiteral("-p ${library_directory}/cpw/mods/bootstraplauncher/1.1.2/bootstraplauncher-1.1.2.jar") } },
          } },
    };

    QString error;
    const auto component = componentFromForgeVersion(document, QStringLiteral("47.4.0"), &error);
    QVERIFY2(component.has_value(), qPrintable(error));

    QCOMPARE(component->value(QStringLiteral("uid")).toString(), QStringLiteral("net.minecraftforge"));
    QCOMPARE(component->value(QStringLiteral("version")).toString(), QStringLiteral("47.4.0"));
    QCOMPARE(component->value(QStringLiteral("mainClass")).toString(),
             QStringLiteral("cpw.mods.bootstraplauncher.BootstrapLauncher"));
    QCOMPARE(component->value(QStringLiteral("minecraftArguments")).toString(),
             QStringLiteral("--launchTarget forgeclient --fml.forgeVersion 47.4.0"));

    const auto jvmArgs = component->value(QStringLiteral("+jvmArgs")).toArray();
    QCOMPARE(jvmArgs.size(), 2);
    QVERIFY(jvmArgs.first().toString().contains(QStringLiteral("${library_directory}")));

    // What the launcher reads back is the same command line, placeholders untouched.
    const auto manifest = VersionJson::parse(*component, &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));
    QCOMPARE(manifest->legacyArguments.first(), QStringLiteral("--launchTarget"));
    QVERIFY(manifest->jvmArguments.contains(QStringLiteral("-DlibraryDirectory=${library_directory}")));
}
void KomorebiLoaderManifestTest::refusesAProfileWithoutAMainClass()
{
    auto profile = fabricProfile();
    profile.remove(QStringLiteral("mainClass"));
    QString error;
    QVERIFY(!componentFromLoaderProfile(profile, QStringLiteral("net.fabricmc.fabric-loader"), QStringLiteral("Fabric Loader"),
                                                 QStringLiteral("0.15.11"), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("main class")));
}

QTEST_GUILESS_MAIN(KomorebiLoaderManifestTest)

#include "KomorebiLoaderManifest_test.moc"
