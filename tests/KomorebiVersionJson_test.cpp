// SPDX-License-Identifier: GPL-3.0-only
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "komorebi/core/version/VersionJson.h"

using namespace Komorebi::Core;

class KomorebiVersionJsonTest : public QObject {
    Q_OBJECT

   private slots:
    void parseManifest();
    void parseRejectsWhatIsNotAManifest();
    void parseComponentManifest();
    void mergeKeepsTheChildsIdentityAndTakesTheParentsDefaults();
    void mergeOrdersArgumentsFromBaseToLoader();
    void mergeRefusesAMissingParent();
    void rulesFollowThePlatform();
    void libraryPathsFollowTheName();
};

void KomorebiVersionJsonTest::parseManifest()
{
    const auto document = QJsonDocument::fromJson(R"({
        "id": "1.20.1",
        "mainClass": "net.minecraft.client.main.Main",
        "type": "release",
        "assets": "5",
        "assetIndex": { "id": "5", "url": "https://example.invalid/5.json", "sha1": "abc", "size": 123 },
        "javaVersion": { "majorVersion": 17, "component": "java-runtime-gamma" },
        "downloads": { "client": { "url": "https://example.invalid/client.jar", "sha1": "def" } },
        "libraries": [
            {
                "name": "org.lwjgl:lwjgl:3.3.1",
                "downloads": { "artifact": { "path": "org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1.jar", "sha1": "1", "size": 10 } }
            },
            {
                "name": "org.lwjgl:lwjgl:3.3.1:natives-windows",
                "rules": [ { "action": "allow", "os": { "name": "windows" } } ],
                "natives": { "windows": "natives-windows" }
            }
        ],
        "arguments": {
            "game": [ "--username", "${auth_player_name}", { "rules": [ { "action": "allow", "os": { "name": "osx" } } ], "value": "-XstartOnFirstThread" } ],
            "jvm": [ "-Djava.library.path=${natives_directory}", "-cp", "${classpath}" ]
        }
    })")
                            .object();

    QString error;
    const auto manifest = VersionJson::parse(document, &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));

    QCOMPARE(manifest->id, QStringLiteral("1.20.1"));
    QCOMPARE(manifest->mainClass, QStringLiteral("net.minecraft.client.main.Main"));
    QCOMPARE(manifest->assetIndexId, QStringLiteral("5"));
    QCOMPARE(manifest->assetIndex.value(QStringLiteral("sha1")).toString(), QStringLiteral("abc"));
    QCOMPARE(manifest->javaMajor, 17);
    QCOMPARE(manifest->javaComponent, QStringLiteral("java-runtime-gamma"));
    QVERIFY(manifest->downloads.contains(QStringLiteral("client")));

    QCOMPARE(manifest->libraries.size(), 2);
    QCOMPARE(manifest->libraries.at(1).nativesClassifier(Platform{ QStringLiteral("windows"), QStringLiteral("x86_64") }),
             QStringLiteral("natives-windows"));

    // The plain strings are kept; the conditional macOS entry is not part of this slice.
    QCOMPARE(manifest->gameArguments,
             QStringList({ QStringLiteral("--username"), QStringLiteral("${auth_player_name}") }));
    QCOMPARE(manifest->jvmArguments.size(), 3);
}

void KomorebiVersionJsonTest::parseComponentManifest()
{
    // What this launcher keeps for every version: a uid/version pair, the majors to pick Java from,
    // extra JVM arguments and the coordinate of the client jar, rather than a Mojang-style id.
    const auto document = QJsonDocument::fromJson(R"({
        "uid": "net.minecraft",
        "version": "26.4-snapshot-7",
        "mainClass": "net.minecraft.client.main.Main",
        "compatibleJavaMajors": [ 25, 21 ],
        "mainJar": { "name": "com.mojang:minecraft:26.4-snapshot-7:client" },
        "+jvmArgs": [ "-Dexample=1" ],
        "minecraftArguments": "--username ${auth_player_name}",
        "libraries": [ { "name": "org.example:one:1.0" } ]
    })")
                            .object();

    QString error;
    const auto manifest = VersionJson::parse(document, &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));
    QCOMPARE(manifest->id, QStringLiteral("26.4-snapshot-7"));
    QCOMPARE(manifest->javaMajor, 25);
    QCOMPARE(manifest->mainJarName, QStringLiteral("com.mojang:minecraft:26.4-snapshot-7:client"));
    QCOMPARE(manifest->jvmArguments, QStringList({ QStringLiteral("-Dexample=1") }));
    QCOMPARE(manifest->libraries.size(), 1);
}
void KomorebiVersionJsonTest::parseRejectsWhatIsNotAManifest()
{
    QString error;
    QVERIFY(!VersionJson::parse(QJsonObject{}, &error).has_value());
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!VersionJson::parse(QJsonObject{ { QStringLiteral("mainClass"), QStringLiteral("x") } }, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("id")));
}

void KomorebiVersionJsonTest::mergeKeepsTheChildsIdentityAndTakesTheParentsDefaults()
{
    const auto parent = *VersionJson::parse(QJsonDocument::fromJson(R"({
        "id": "1.20.1",
        "mainClass": "net.minecraft.client.main.Main",
        "assets": "5",
        "javaVersion": { "majorVersion": 17 },
        "libraries": [ { "name": "com.example:base:1" } ]
    })")
                                                .object());
    const auto child = *VersionJson::parse(QJsonDocument::fromJson(R"({
        "id": "1.20.1-forge-47.4.28",
        "inheritsFrom": "1.20.1",
        "mainClass": "cpw.mods.bootstraplauncher.BootstrapLauncher",
        "libraries": [ { "name": "net.minecraftforge:forge:47.4.28" } ]
    })")
                                               .object());

    const auto merged = mergeChain({ child, parent });
    QVERIFY(merged.has_value());
    QCOMPARE(merged->id, QStringLiteral("1.20.1-forge-47.4.28"));
    QVERIFY(merged->inheritsFrom.isEmpty());
    QCOMPARE(merged->mainClass, QStringLiteral("cpw.mods.bootstraplauncher.BootstrapLauncher"));
    QCOMPARE(merged->assets, QStringLiteral("5"));
    QCOMPARE(merged->javaMajor, 17);
    QCOMPARE(merged->libraries.size(), 2);
    QCOMPARE(merged->libraries.at(0).name, QStringLiteral("net.minecraftforge:forge:47.4.28"));
    QCOMPARE(merged->libraries.at(1).name, QStringLiteral("com.example:base:1"));
}

void KomorebiVersionJsonTest::mergeOrdersArgumentsFromBaseToLoader()
{
    const auto parent = *VersionJson::parse(QJsonDocument::fromJson(R"({
        "id": "1.20.1",
        "arguments": { "game": [ "--gameDir", "${game_directory}" ], "jvm": [ "-cp", "${classpath}" ] },
        "minecraftArguments": "--username ${auth_player_name}"
    })")
                                                .object());
    const auto child = *VersionJson::parse(QJsonDocument::fromJson(R"({
        "id": "loader",
        "inheritsFrom": "1.20.1",
        "arguments": { "game": [ "--launchTarget", "forgeclient" ], "jvm": [ "-Dfml.ignoreInvalidMinecraftCertificates=true" ] },
        "minecraftArguments": "--username ${auth_player_name} --launchTarget forgeclient"
    })")
                                               .object());

    const auto merged = mergeChain({ child, parent });
    QVERIFY(merged.has_value());

    QCOMPARE(merged->gameArguments,
             QStringList({ QStringLiteral("--gameDir"), QStringLiteral("${game_directory}"), QStringLiteral("--launchTarget"),
                           QStringLiteral("forgeclient") }));
    QCOMPARE(merged->jvmArguments,
             QStringList({ QStringLiteral("-cp"), QStringLiteral("${classpath}"),
                           QStringLiteral("-Dfml.ignoreInvalidMinecraftCertificates=true") }));
    // The loader rewrote the whole line, so its version wins rather than being appended to the base's.
    QCOMPARE(merged->legacyArguments,
             QStringList({ QStringLiteral("--username"), QStringLiteral("${auth_player_name}"), QStringLiteral("--launchTarget"),
                           QStringLiteral("forgeclient") }));
}

void KomorebiVersionJsonTest::mergeRefusesAMissingParent()
{
    const auto child = *VersionJson::parse(QJsonDocument::fromJson(R"({ "id": "loader", "inheritsFrom": "1.20.1" })").object());
    QString error;
    QVERIFY(!mergeChain({ child }, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("1.20.1")));
}

void KomorebiVersionJsonTest::rulesFollowThePlatform()
{
    LibraryEntry windowsOnly;
    windowsOnly.rules = { Rule{ QStringLiteral("allow"), QStringLiteral("windows"), QString() } };
    QVERIFY(windowsOnly.appliesTo(Platform{ QStringLiteral("windows"), QStringLiteral("x86_64") }));
    QVERIFY(!windowsOnly.appliesTo(Platform{ QStringLiteral("linux"), QStringLiteral("x86_64") }));

    LibraryEntry notLinux;
    notLinux.rules = { Rule{ QStringLiteral("allow"), QString(), QString() },
                       Rule{ QStringLiteral("disallow"), QStringLiteral("linux"), QString() } };
    QVERIFY(notLinux.appliesTo(Platform{ QStringLiteral("windows"), QStringLiteral("x86_64") }));
    QVERIFY(!notLinux.appliesTo(Platform{ QStringLiteral("linux"), QStringLiteral("x86_64") }));

    LibraryEntry armOnly;
    armOnly.rules = { Rule{ QStringLiteral("allow"), QString(), QStringLiteral("arm64") } };
    QVERIFY(armOnly.appliesTo(Platform{ QStringLiteral("windows"), QStringLiteral("arm64") }));
    QVERIFY(!armOnly.appliesTo(Platform{ QStringLiteral("windows"), QStringLiteral("x86_64") }));

    LibraryEntry anywhere;
    QVERIFY(anywhere.appliesTo(Platform{ QStringLiteral("osx"), QStringLiteral("arm64") }));
}

void KomorebiVersionJsonTest::libraryPathsFollowTheName()
{
    QCOMPARE(libraryRelativePath(QStringLiteral("org.lwjgl:lwjgl:3.3.1")),
             QStringLiteral("org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1.jar"));
    QCOMPARE(libraryRelativePath(QStringLiteral("org.lwjgl:lwjgl:3.3.1:natives-windows")),
             QStringLiteral("org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows.jar"));
    QVERIFY(libraryRelativePath(QStringLiteral("not-a-coordinate")).isEmpty());

    LibraryEntry withPath;
    withPath.name = QStringLiteral("org.lwjgl:lwjgl:3.3.1");
    withPath.path = QStringLiteral("given/by/the/manifest.jar");
    QCOMPARE(libraryPath(withPath), QStringLiteral("given/by/the/manifest.jar"));

    LibraryEntry withoutPath;
    withoutPath.name = QStringLiteral("org.lwjgl:lwjgl:3.3.1");
    QCOMPARE(libraryPath(withoutPath), QStringLiteral("org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1.jar"));
}

QTEST_GUILESS_MAIN(KomorebiVersionJsonTest)

#include "KomorebiVersionJson_test.moc"
