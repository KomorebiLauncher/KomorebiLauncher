// SPDX-License-Identifier: GPL-3.0-only
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "komorebi/core/launch/LaunchPlan.h"
#include "komorebi/core/version/VersionJson.h"

using namespace Komorebi::Core;

class KomorebiLaunchPlanTest : public QObject {
    Q_OBJECT

   private slots:
    void modernVersionGetsItsArgumentsSubstituted();
    void oldVersionGetsTheClassPathToldToIt();
    void classPathSkipsLibrariesForOtherPlatforms();
    void missingPiecesAreReported();
};

namespace {

const Platform kWindows{ QStringLiteral("windows"), QStringLiteral("x86_64") };

VersionJson modernManifest()
{
    return *VersionJson::parse(QJsonDocument::fromJson(R"({
        "id": "1.20.1",
        "mainClass": "net.minecraft.client.main.Main",
        "assetIndex": { "id": "5" },
        "libraries": [
            { "name": "org.example:one:1.0",
              "downloads": { "artifact": { "path": "org/example/one/1.0/one-1.0.jar" } } },
            { "name": "org.example:windows:1.0",
              "downloads": { "artifact": { "path": "org/example/windows/1.0/windows-1.0.jar" } },
              "rules": [ { "action": "allow", "os": { "name": "windows" } } ] },
            { "name": "org.example:linux:1.0",
              "downloads": { "artifact": { "path": "org/example/linux/1.0/linux-1.0.jar" } },
              "rules": [ { "action": "allow", "os": { "name": "linux" } } ] }
        ],
        "arguments": {
            "jvm": [ "-Djava.library.path=${natives_directory}", "-cp", "${classpath}" ],
            "game": [ "--username", "${auth_player_name}", "--gameDir", "${game_directory}",
                      "--assetsDir", "${assets_root}", "--assetIndex", "${assets_index_name}" ]
        }
    })")
                                     .object());
}

LaunchOptions optionsFor()
{
    LaunchOptions options;
    options.javaPath = QStringLiteral("C:/java/bin/java.exe");
    options.gameDirectory = QStringLiteral("C:/instances/test");
    options.assetsDirectory = QStringLiteral("C:/assets");
    options.librariesDirectory = QStringLiteral("C:/libs");
    options.nativesDirectory = QStringLiteral("C:/instances/test/natives");
    options.clientJarPath = QStringLiteral("C:/instances/test/1.20.1.jar");
    options.versionName = QStringLiteral("1.20.1");
    options.username = QStringLiteral("Steve");
    options.uuid = QStringLiteral("uuid-1");
    options.accessToken = QStringLiteral("token-1");
    return options;
}

}  // namespace

void KomorebiLaunchPlanTest::modernVersionGetsItsArgumentsSubstituted()
{
    const auto plan = buildLaunchPlan(modernManifest(), optionsFor(), kWindows);
    QVERIFY2(plan.ok(), qPrintable(plan.problems.join(QStringLiteral("; "))));

    QCOMPARE(plan.program, QStringLiteral("C:/java/bin/java.exe"));
    QCOMPARE(plan.workingDirectory, QStringLiteral("C:/instances/test"));

    QVERIFY(plan.arguments.contains(QStringLiteral("-Xmx2048m")));
    QVERIFY(plan.arguments.contains(QStringLiteral("-Djava.library.path=C:/instances/test/natives")));

    const QString classpath = QStringLiteral("C:/libs/org/example/one/1.0/one-1.0.jar;"
                                             "C:/libs/org/example/windows/1.0/windows-1.0.jar;"
                                             "C:/instances/test/1.20.1.jar");
    QVERIFY(plan.arguments.contains(classpath));

    // The main class comes after the JVM arguments and before the game's own.
    const int mainClass = plan.arguments.indexOf(QStringLiteral("net.minecraft.client.main.Main"));
    QVERIFY(mainClass > 0);
    QCOMPARE(plan.arguments.at(mainClass + 1), QStringLiteral("--username"));
    QCOMPARE(plan.arguments.at(mainClass + 2), QStringLiteral("Steve"));
    QVERIFY(plan.arguments.contains(QStringLiteral("C:/instances/test")));
    QVERIFY(plan.arguments.contains(QStringLiteral("5")));

    for (const auto& argument : plan.arguments)
        QVERIFY2(!argument.contains(QStringLiteral("${")), qPrintable(argument));
}

void KomorebiLaunchPlanTest::oldVersionGetsTheClassPathToldToIt()
{
    const auto manifest = *VersionJson::parse(QJsonDocument::fromJson(R"({
        "id": "1.7.10",
        "mainClass": "net.minecraft.client.main.Main",
        "minecraftArguments": "--username ${auth_player_name} --gameDir ${game_directory} --assetsDir ${assets_root}",
        "libraries": [ { "name": "org.example:one:1.0",
                         "downloads": { "artifact": { "path": "org/example/one/1.0/one-1.0.jar" } } } ]
    })")
                                                       .object());

    const auto plan = buildLaunchPlan(manifest, optionsFor(), kWindows);
    QVERIFY2(plan.ok(), qPrintable(plan.problems.join(QStringLiteral("; "))));

    QVERIFY(plan.arguments.contains(QStringLiteral("-Djava.library.path=C:/instances/test/natives")));
    QVERIFY(plan.arguments.contains(QStringLiteral("C:/libs/org/example/one/1.0/one-1.0.jar;C:/instances/test/1.20.1.jar")));

    const int gameDir = plan.arguments.indexOf(QStringLiteral("--gameDir"));
    QVERIFY(gameDir > 0);
    QCOMPARE(plan.arguments.at(gameDir + 1), QStringLiteral("C:/instances/test"));
}

void KomorebiLaunchPlanTest::classPathSkipsLibrariesForOtherPlatforms()
{
    const auto entries = classpathEntries(modernManifest(), optionsFor(), kWindows);
    QCOMPARE(entries.size(), 3);
    QVERIFY(!entries.join(QChar(';')).contains(QStringLiteral("linux-1.0.jar")));
    QCOMPARE(entries.last(), QStringLiteral("C:/instances/test/1.20.1.jar"));
}

void KomorebiLaunchPlanTest::missingPiecesAreReported()
{
    auto options = optionsFor();
    options.javaPath.clear();
    auto plan = buildLaunchPlan(modernManifest(), options, kWindows);
    QVERIFY(!plan.ok());
    QVERIFY(plan.problems.join(QLatin1Char(' ')).contains(QStringLiteral("Java")));

    auto manifest = modernManifest();
    manifest.mainClass.clear();
    plan = buildLaunchPlan(manifest, optionsFor(), kWindows);
    QVERIFY(!plan.ok());
    QVERIFY(plan.problems.join(QLatin1Char(' ')).contains(QStringLiteral("class")));
}

QTEST_GUILESS_MAIN(KomorebiLaunchPlanTest)

#include "KomorebiLaunchPlan_test.moc"
