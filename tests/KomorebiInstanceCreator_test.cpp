// SPDX-License-Identifier: GPL-3.0-only
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "KomorebiTestServer.h"
#include "komorebi/core/instance/InstanceFolder.h"
#include "komorebi/core/version/VersionJson.h"
#include "komorebi/services/download/InstanceFiles.h"
#include "komorebi/services/install/InstanceCreator.h"

using namespace Komorebi::Core;
using namespace Komorebi::Services;

namespace {

QString sha1Hex(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
}

QByteArray readAll(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/// Builds a Mojang version document whose files all come from the given server.
QByteArray versionDocument(const QByteArray& clientJar, const QByteArray& libraryJar, quint16 port)
{
    const auto url = [port](const QString& path) {
        return QStringLiteral("http://127.0.0.1:%1%2").arg(port).arg(path);
    };
    const QJsonObject client{ { QStringLiteral("sha1"), sha1Hex(clientJar) },
                              { QStringLiteral("size"), clientJar.size() },
                              { QStringLiteral("url"), url(QStringLiteral("/client.jar")) } };
    const QJsonObject artifact{ { QStringLiteral("path"), QStringLiteral("org/example/one/1.0/one-1.0.jar") },
                                { QStringLiteral("sha1"), sha1Hex(libraryJar) },
                                { QStringLiteral("size"), libraryJar.size() },
                                { QStringLiteral("url"), url(QStringLiteral("/lib.jar")) } };
    const QJsonObject library{ { QStringLiteral("name"), QStringLiteral("org.example:one:1.0") },
                               { QStringLiteral("downloads"), QJsonObject{ { QStringLiteral("artifact"), artifact } } } };
    const QJsonArray gameArguments{ QStringLiteral("--username"), QStringLiteral("${auth_player_name}"),
                                    QStringLiteral("--version"), QStringLiteral("${version_name}") };
    const QJsonObject document{
        { QStringLiteral("id"), QStringLiteral("1.20.1") },
        { QStringLiteral("type"), QStringLiteral("release") },
        { QStringLiteral("releaseTime"), QStringLiteral("2023-06-12T13:25:51+00:00") },
        { QStringLiteral("mainClass"), QStringLiteral("net.minecraft.client.main.Main") },
        { QStringLiteral("downloads"), QJsonObject{ { QStringLiteral("client"), client } } },
        { QStringLiteral("javaVersion"),
          QJsonObject{ { QStringLiteral("majorVersion"), 17 }, { QStringLiteral("component"), QStringLiteral("java-runtime-gamma") } } },
        { QStringLiteral("arguments"), QJsonObject{ { QStringLiteral("game"), gameArguments } } },
        { QStringLiteral("libraries"), QJsonArray{ library } },
    };
    return QJsonDocument(document).toJson();
}

QByteArray manifestDocument(quint16 port, const QByteArray& versionBytes)
{
    const QJsonObject version{ { QStringLiteral("id"), QStringLiteral("1.20.1") },
                               { QStringLiteral("type"), QStringLiteral("release") },
                               { QStringLiteral("url"), QStringLiteral("http://127.0.0.1:%1/version.json").arg(port) },
                               { QStringLiteral("sha1"), sha1Hex(versionBytes) } };
    const QJsonObject manifest{ { QStringLiteral("latest"),
                                  QJsonObject{ { QStringLiteral("release"), QStringLiteral("1.20.1") },
                                               { QStringLiteral("snapshot"), QStringLiteral("24w14a") } } },
                                { QStringLiteral("versions"), QJsonArray{ version } } };
    return QJsonDocument(manifest).toJson();
}

}  // namespace

class KomorebiInstanceCreatorTest : public QObject {
    Q_OBJECT

   private slots:
    void createsAVanillaInstanceAndItsFiles();
    void canCreateWithoutFetchingFiles();
    void refusesAVersionThatIsNotInTheList();
    void createsAFabricInstance();
};

void KomorebiInstanceCreatorTest::createsAVanillaInstanceAndItsFiles()
{
    QTemporaryDir data;
    QVERIFY(data.isValid());
    KomorebiTestServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    const QByteArray clientJar = QByteArrayLiteral("the client jar");
    const QByteArray libraryJar = QByteArrayLiteral("a library");
    const QByteArray versionBytes = versionDocument(clientJar, libraryJar, server.serverPort());
    server.files.insert("/manifest.json", manifestDocument(server.serverPort(), versionBytes));
    server.files.insert("/version.json", versionBytes);
    server.files.insert("/client.jar", clientJar);
    server.files.insert("/lib.jar", libraryJar);

    QNetworkAccessManager network;
    InstanceCreator creator(&network);
    QSignalSpy finished(&creator, &InstanceCreator::finished);
    QSignalSpy failed(&creator, &InstanceCreator::failed);
    InstanceCreateRequest request;
    request.dataRoot = data.path();
    request.name = QStringLiteral("My World");
    request.minecraftVersion = QStringLiteral("1.20.1");
    request.versionManifestUrl = QStringLiteral("http://127.0.0.1:%1/manifest.json").arg(server.serverPort());
    creator.start(request);

    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1 || failed.count() == 1, 20000);
    QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.first().at(0).toString()));

    const QString instanceDirectory = finished.first().at(0).toString();
    QCOMPARE(instanceDirectory, QDir(data.path()).filePath(QStringLiteral("instances/My World")));
    QVERIFY(QFile::exists(QDir(instanceDirectory).filePath(QStringLiteral("mmc-pack.json"))));

    const auto folder = Komorebi::Core::InstanceFolder::read(instanceDirectory);
    QVERIFY(folder.has_value());
    QCOMPARE(folder->name, QStringLiteral("My World"));
    QCOMPARE(folder->minecraftVersion(), QStringLiteral("1.20.1"));

    // The version was kept as component metadata, and reads back as a launchable version.
    QFile meta(QDir(data.path()).filePath(QStringLiteral("meta/net.minecraft/1.20.1.json")));
    QVERIFY(meta.open(QIODevice::ReadOnly));
    QString error;
    const auto manifest = Komorebi::Core::VersionJson::parse(QJsonDocument::fromJson(meta.readAll()).object(), &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));
    QCOMPARE(manifest->id, QStringLiteral("1.20.1"));
    QCOMPARE(manifest->javaMajor, 17);

    QCOMPARE(readAll(QDir(data.path()).filePath(QStringLiteral("libraries/com/mojang/minecraft/1.20.1/minecraft-1.20.1-client.jar"))),
             clientJar);
    QCOMPARE(readAll(QDir(data.path()).filePath(QStringLiteral("libraries/org/example/one/1.0/one-1.0.jar"))), libraryJar);
    QVERIFY(missingInstanceFiles(data.path(), instanceDirectory, &error).isEmpty());
}

void KomorebiInstanceCreatorTest::canCreateWithoutFetchingFiles()
{
    QTemporaryDir data;
    QVERIFY(data.isValid());
    KomorebiTestServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    const QByteArray versionBytes = versionDocument(QByteArrayLiteral("client"), QByteArrayLiteral("library"), server.serverPort());
    server.files.insert("/manifest.json", manifestDocument(server.serverPort(), versionBytes));
    server.files.insert("/version.json", versionBytes);

    QNetworkAccessManager network;
    InstanceCreator creator(&network);
    QSignalSpy finished(&creator, &InstanceCreator::finished);
    QSignalSpy failed(&creator, &InstanceCreator::failed);
    InstanceCreateRequest request;
    request.dataRoot = data.path();
    request.name = QStringLiteral("Quick");
    request.minecraftVersion = QStringLiteral("1.20.1");
    request.versionManifestUrl = QStringLiteral("http://127.0.0.1:%1/manifest.json").arg(server.serverPort());
    request.ensureFiles = false;
    creator.start(request);

    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1 || failed.count() == 1, 20000);
    QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.first().at(0).toString()));
    QVERIFY(!QFile::exists(QDir(data.path()).filePath(QStringLiteral("libraries/org/example/one/1.0/one-1.0.jar"))));
    QVERIFY(QFile::exists(QDir(data.path()).filePath(QStringLiteral("meta/net.minecraft/1.20.1.json"))));
}

void KomorebiInstanceCreatorTest::createsAFabricInstance()
{
    QTemporaryDir data;
    QVERIFY(data.isValid());
    KomorebiTestServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    const auto base = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    const QByteArray clientJar = QByteArrayLiteral("client");
    const QByteArray libraryJar = QByteArrayLiteral("library");
    const QByteArray fabricJar = QByteArrayLiteral("fabric loader");
    const QByteArray versionBytes = versionDocument(clientJar, libraryJar, server.serverPort());
    server.files.insert("/manifest.json", manifestDocument(server.serverPort(), versionBytes));
    server.files.insert("/version.json", versionBytes);
    server.files.insert("/client.jar", clientJar);
    server.files.insert("/lib.jar", libraryJar);

    const QJsonObject loaderList{ { QStringLiteral("loader"),
                                    QJsonObject{ { QStringLiteral("version"), QStringLiteral("0.15.11") },
                                                 { QStringLiteral("stable"), true } } } };
    server.files.insert("/v2/versions/loader/1.20.1", QJsonDocument(QJsonArray{ loaderList }).toJson());
    const QJsonObject profile{
        { QStringLiteral("id"), QStringLiteral("fabric-loader-0.15.11-1.20.1") },
        { QStringLiteral("mainClass"), QStringLiteral("net.fabricmc.loader.impl.launch.knot.KnotClient") },
        { QStringLiteral("libraries"),
          QJsonArray{ QJsonObject{ { QStringLiteral("name"), QStringLiteral("org.ow2.asm:asm:9.10.1") },
                                   { QStringLiteral("url"), base + QStringLiteral("/maven/") },
                                   { QStringLiteral("sha1"), sha1Hex(fabricJar) },
                                   { QStringLiteral("size"), fabricJar.size() } } } },
        { QStringLiteral("arguments"), QJsonObject{ { QStringLiteral("jvm"), QJsonArray{} } } },
    };
    server.files.insert("/v2/versions/loader/1.20.1/0.15.11/profile/json", QJsonDocument(profile).toJson());
    server.files.insert("/maven/org/ow2/asm/asm/9.10.1/asm-9.10.1.jar", fabricJar);

    QNetworkAccessManager network;
    InstanceCreator creator(&network);
    QSignalSpy finished(&creator, &InstanceCreator::finished);
    QSignalSpy failed(&creator, &InstanceCreator::failed);
    InstanceCreateRequest request;
    request.dataRoot = data.path();
    request.name = QStringLiteral("Fabric World");
    request.minecraftVersion = QStringLiteral("1.20.1");
    request.versionManifestUrl = base + QStringLiteral("/manifest.json");
    request.loaderMetaUrl = base;
    request.loader = QStringLiteral("fabric");
    creator.start(request);

    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1 || failed.count() == 1, 20000);
    QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.first().at(0).toString()));

    const QString instanceDirectory = finished.first().at(0).toString();
    QFile pack(QDir(instanceDirectory).filePath(QStringLiteral("mmc-pack.json")));
    QVERIFY(pack.open(QIODevice::ReadOnly));
    const auto components = QJsonDocument::fromJson(pack.readAll()).object().value(QStringLiteral("components")).toArray();
    QCOMPARE(components.size(), 2);
    QCOMPARE(components.at(1).toObject().value(QStringLiteral("uid")).toString(),
             QStringLiteral("net.fabricmc.fabric-loader"));
    QVERIFY(QFile::exists(QDir(data.path()).filePath(QStringLiteral("meta/net.fabricmc.fabric-loader/0.15.11.json"))));

    QString error;
    QVERIFY2(missingInstanceFiles(data.path(), instanceDirectory, &error).isEmpty(), qPrintable(error));
    QCOMPARE(readAll(QDir(data.path()).filePath(QStringLiteral("libraries/org/ow2/asm/asm/9.10.1/asm-9.10.1.jar"))),
             fabricJar);
}
void KomorebiInstanceCreatorTest::refusesAVersionThatIsNotInTheList()
{
    QTemporaryDir data;
    QVERIFY(data.isValid());
    KomorebiTestServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.files.insert("/manifest.json", manifestDocument(server.serverPort(), QByteArrayLiteral("{}")));

    QNetworkAccessManager network;
    InstanceCreator creator(&network);
    QSignalSpy finished(&creator, &InstanceCreator::finished);
    QSignalSpy failed(&creator, &InstanceCreator::failed);
    InstanceCreateRequest request;
    request.dataRoot = data.path();
    request.name = QStringLiteral("Nope");
    request.minecraftVersion = QStringLiteral("1.0.0-not-there");
    request.versionManifestUrl = QStringLiteral("http://127.0.0.1:%1/manifest.json").arg(server.serverPort());
    creator.start(request);

    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1 || failed.count() == 1, 20000);
    QVERIFY(finished.isEmpty());
    QVERIFY(failed.first().at(0).toString().contains(QStringLiteral("not in the version list")));
}

QTEST_GUILESS_MAIN(KomorebiInstanceCreatorTest)

#include "KomorebiInstanceCreator_test.moc"
