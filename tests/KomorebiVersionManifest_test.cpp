// SPDX-License-Identifier: GPL-3.0-only
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "komorebi/core/version/VersionJson.h"
#include "komorebi/core/version/VersionManifest.h"

using namespace Komorebi::Core;

namespace {

QJsonObject manifestDocument()
{
    return QJsonObject{
        { QStringLiteral("latest"),
          QJsonObject{ { QStringLiteral("release"), QStringLiteral("1.20.1") },
                       { QStringLiteral("snapshot"), QStringLiteral("24w14a") } } },
        { QStringLiteral("versions"),
          QJsonArray{
              QJsonObject{ { QStringLiteral("id"), QStringLiteral("1.20.1") },
                           { QStringLiteral("type"), QStringLiteral("release") },
                           { QStringLiteral("url"), QStringLiteral("https://piston-meta.mojang.com/v1/packages/aa/1.20.1.json") },
                           { QStringLiteral("sha1"), QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") },
                           { QStringLiteral("releaseTime"), QStringLiteral("2023-06-12T13:25:51+00:00") } },
              QJsonObject{ { QStringLiteral("id"), QStringLiteral("24w14a") },
                           { QStringLiteral("type"), QStringLiteral("snapshot") },
                           { QStringLiteral("url"), QStringLiteral("https://piston-meta.mojang.com/v1/packages/bb/24w14a.json") },
                           { QStringLiteral("sha1"), QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb") },
                           { QStringLiteral("releaseTime"), QStringLiteral("2024-04-03T12:00:00+00:00") } },
          } },
    };
}

QJsonObject mojangVersionDocument()
{
    const QJsonObject client{ { QStringLiteral("sha1"), QStringLiteral("0c3ec587af28e5a785c0b4a7b8a30f9a8f78f838") },
                              { QStringLiteral("size"), 23028853 },
                              { QStringLiteral("url"), QStringLiteral("https://piston-data.mojang.com/v1/objects/0c3ec587/client.jar") } };

    const QJsonObject assetIndex{ { QStringLiteral("id"), QStringLiteral("5") },
                                  { QStringLiteral("sha1"), QStringLiteral("ada52cc672270e8dc69e912b2ba0ea9c141ed664") },
                                  { QStringLiteral("size"), 413067 },
                                  { QStringLiteral("url"), QStringLiteral("https://piston-meta.mojang.com/v1/packages/ada52cc/5.json") } };

    const QJsonObject javaVersion{ { QStringLiteral("component"), QStringLiteral("java-runtime-gamma") },
                                   { QStringLiteral("majorVersion"), 17 } };

    const QJsonObject demoArgument{
        { QStringLiteral("rules"),
          QJsonArray{ QJsonObject{ { QStringLiteral("action"), QStringLiteral("allow") },
                                   { QStringLiteral("features"), QJsonObject{ { QStringLiteral("is_demo_user"), true } } } } } },
        { QStringLiteral("value"), QJsonArray{ QStringLiteral("--demo") } }
    };
    const QJsonArray gameArguments{ QStringLiteral("--username"), QStringLiteral("${auth_player_name}"),
                                    QStringLiteral("--version"), QStringLiteral("${version_name}"), demoArgument };
    const QJsonArray jvmArguments{ QStringLiteral("-Djava.library.path=${natives_directory}") };
    const QJsonObject arguments{ { QStringLiteral("game"), gameArguments }, { QStringLiteral("jvm"), jvmArguments } };

    const QJsonObject artifact{ { QStringLiteral("path"), QStringLiteral("org/example/one/1.0/one-1.0.jar") },
                                { QStringLiteral("sha1"), QStringLiteral("cccccccccccccccccccccccccccccccccccccccc") },
                                { QStringLiteral("size"), 123 },
                                { QStringLiteral("url"),
                                  QStringLiteral("https://libraries.minecraft.net/org/example/one/1.0/one-1.0.jar") } };
    const QJsonObject library{ { QStringLiteral("name"), QStringLiteral("org.example:one:1.0") },
                               { QStringLiteral("downloads"), QJsonObject{ { QStringLiteral("artifact"), artifact } } } };

    return QJsonObject{
        { QStringLiteral("id"), QStringLiteral("1.20.1") },
        { QStringLiteral("type"), QStringLiteral("release") },
        { QStringLiteral("releaseTime"), QStringLiteral("2023-06-12T13:25:51+00:00") },
        { QStringLiteral("mainClass"), QStringLiteral("net.minecraft.client.main.Main") },
        { QStringLiteral("downloads"), QJsonObject{ { QStringLiteral("client"), client } } },
        { QStringLiteral("assetIndex"), assetIndex },
        { QStringLiteral("javaVersion"), javaVersion },
        { QStringLiteral("arguments"), arguments },
        { QStringLiteral("libraries"), QJsonArray{ library } },
    };
}

}  // namespace

class KomorebiVersionManifestTest : public QObject {
    Q_OBJECT

   private slots:
    void readsTheVersionList();
    void convertsAVersionIntoComponentMetadata();
    void refusesAVersionWithoutAClientJar();
};

void KomorebiVersionManifestTest::readsTheVersionList()
{
    QString error;
    const auto manifest = parseVersionManifest(manifestDocument(), &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));
    QCOMPARE(manifest->latestRelease, QStringLiteral("1.20.1"));
    QCOMPARE(manifest->latestSnapshot, QStringLiteral("24w14a"));
    QCOMPARE(manifest->versions.size(), 2);
    QCOMPARE(manifest->versions.first().id, QStringLiteral("1.20.1"));
    QCOMPARE(manifest->versions.first().url, QStringLiteral("https://piston-meta.mojang.com/v1/packages/aa/1.20.1.json"));
}

void KomorebiVersionManifestTest::convertsAVersionIntoComponentMetadata()
{
    QString error;
    const auto component = componentFromMojangVersion(mojangVersionDocument(), &error);
    QVERIFY2(component.has_value(), qPrintable(error));

    QCOMPARE(component->value(QStringLiteral("formatVersion")).toInt(), 1);
    QCOMPARE(component->value(QStringLiteral("uid")).toString(), QStringLiteral("net.minecraft"));
    QCOMPARE(component->value(QStringLiteral("version")).toString(), QStringLiteral("1.20.1"));
    QCOMPARE(component->value(QStringLiteral("type")).toString(), QStringLiteral("release"));
    QCOMPARE(component->value(QStringLiteral("mainClass")).toString(), QStringLiteral("net.minecraft.client.main.Main"));
    QCOMPARE(component->value(QStringLiteral("releaseTime")).toString(), QStringLiteral("12/06/2023 13:25:51"));

    const auto mainJar = component->value(QStringLiteral("mainJar")).toObject();
    QCOMPARE(mainJar.value(QStringLiteral("name")).toString(), QStringLiteral("com.mojang:minecraft:1.20.1:client"));
    QVERIFY(!mainJar.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("artifact")).toObject().isEmpty());

    QCOMPARE(component->value(QStringLiteral("assetIndex")).toObject().value(QStringLiteral("id")).toString(), QStringLiteral("5"));
    QCOMPARE(component->value(QStringLiteral("compatibleJavaMajors")).toArray().first().toInt(), 17);
    QCOMPARE(component->value(QStringLiteral("compatibleJavaName")).toString(), QStringLiteral("java-runtime-gamma"));

    // The game's own arguments become the legacy line, without the rule-carrying demo argument.
    const QString arguments = component->value(QStringLiteral("minecraftArguments")).toString();
    QVERIFY(arguments.contains(QStringLiteral("--username ${auth_player_name}")));
    QVERIFY(!arguments.contains(QStringLiteral("--demo")));

    // Quick play is a trait the launcher decides from the version, and modern versions have it.
    const auto traits = component->value(QStringLiteral("+traits")).toArray();
    QVERIFY(traits.contains(QJsonValue(QStringLiteral("feature:is_quick_play_multiplayer"))));

    // The component has to read back as the version the launcher launches.
    const auto manifest = VersionJson::parse(*component, &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));
    QCOMPARE(manifest->id, QStringLiteral("1.20.1"));
    QCOMPARE(manifest->javaMajor, 17);
    QCOMPARE(manifest->mainJarName, QStringLiteral("com.mojang:minecraft:1.20.1:client"));
    QCOMPARE(manifest->downloads.value(QStringLiteral("client")).toObject().value(QStringLiteral("url")).toString(),
             QStringLiteral("https://piston-data.mojang.com/v1/objects/0c3ec587/client.jar"));
    QCOMPARE(manifest->legacyArguments.value(0), QStringLiteral("--username"));
}

void KomorebiVersionManifestTest::refusesAVersionWithoutAClientJar()
{
    auto document = mojangVersionDocument();
    document.remove(QStringLiteral("downloads"));
    QString error;
    QVERIFY(!componentFromMojangVersion(document, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("client jar")));
}

QTEST_GUILESS_MAIN(KomorebiVersionManifestTest)

#include "KomorebiVersionManifest_test.moc"
