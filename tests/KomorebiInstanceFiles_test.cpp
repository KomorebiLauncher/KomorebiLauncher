// SPDX-License-Identifier: GPL-3.0-only
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "komorebi/core/version/VersionJson.h"
#include "komorebi/services/download/InstanceFiles.h"

using namespace Komorebi::Services;

namespace {

QString sha1Hex(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
}

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

void writeJson(const QString& path, const QJsonObject& object)
{
    writeFile(path, QJsonDocument(object).toJson());
}

}  // namespace

class KomorebiInstanceFilesTest : public QObject {
    Q_OBJECT

   private slots:
    void listsWhatIsMissingAndSkipsWhatIsRight();
    void reportsAnInstanceWithoutManifests();
    void asksForNativeClassifiers();
};

void KomorebiInstanceFilesTest::listsWhatIsMissingAndSkipsWhatIsRight()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QDir data(temp.path());

    const QByteArray presentBytes = QByteArrayLiteral("a library that is already there");
    const QByteArray wrongBytes = QByteArrayLiteral("a different build of the same library");
    const QByteArray absentBytes = QByteArrayLiteral("the library nobody has yet");
    const QByteArray clientBytes = QByteArrayLiteral("the game's own jar");
    const QByteArray lwjglBytes = QByteArrayLiteral("the graphics library");

    // What is already on disk: one correct library and one whose bytes are not what the manifest says.
    writeFile(data.filePath(QStringLiteral("libraries/com/example/present/1.0/present-1.0.jar")), presentBytes);
    writeFile(data.filePath(QStringLiteral("libraries/com/example/wrong/1.0/wrong-1.0.jar")), wrongBytes);

    const auto artifact = [](const QString& group, const QString& name, const QByteArray& bytes, bool withPath = true) {
        QJsonObject artifactObject{ { QStringLiteral("url"),
                                      QStringLiteral("https://libraries.minecraft.net/%1/%2/1.0/%2-1.0.jar").arg(group, name) },
                                    { QStringLiteral("sha1"), sha1Hex(bytes) },
                                    { QStringLiteral("size"), bytes.size() } };
        if (withPath)
            artifactObject.insert(QStringLiteral("path"), QStringLiteral("%1/%2/1.0/%2-1.0.jar").arg(group, name));
        return QJsonObject{ { QStringLiteral("name"), QStringLiteral("%1:%2:1.0").arg(group, name) },
                            { QStringLiteral("downloads"), QJsonObject{ { QStringLiteral("artifact"), artifactObject } } } };
    };

    writeJson(data.filePath(QStringLiteral("meta/net.minecraft/1.20.1.json")),
              QJsonObject{ { QStringLiteral("uid"), QStringLiteral("net.minecraft") },
                           { QStringLiteral("version"), QStringLiteral("1.20.1") },
                           { QStringLiteral("mainClass"), QStringLiteral("net.minecraft.client.main.Main") },
                           { QStringLiteral("mainJar"), QJsonObject{ { QStringLiteral("name"),
                                                                      QStringLiteral("com.mojang:minecraft:1.20.1:client") } } },
                           { QStringLiteral("compatibleJavaMajors"), QJsonArray{ 17 } },
                           { QStringLiteral("downloads"),
                             QJsonObject{ { QStringLiteral("client"),
                                            QJsonObject{ { QStringLiteral("url"),
                                                           QStringLiteral("https://piston-data.mojang.com/v1/objects/abc/client.jar") },
                                                         { QStringLiteral("sha1"), sha1Hex(clientBytes) },
                                                         { QStringLiteral("size"), clientBytes.size() } } } } },
                           { QStringLiteral("libraries"),
                             QJsonArray{ artifact(QStringLiteral("com/example"), QStringLiteral("present"), presentBytes),
                                         // The manifest names different bytes than the file on disk holds, so this one has to be replaced.
                                         artifact(QStringLiteral("com/example"), QStringLiteral("wrong"), absentBytes),
                                         artifact(QStringLiteral("com/example"), QStringLiteral("absent"), absentBytes,
                                                  /*withPath=*/false) } } });

    writeJson(data.filePath(QStringLiteral("meta/org.lwjgl3/3.3.1.json")),
              QJsonObject{ { QStringLiteral("uid"), QStringLiteral("org.lwjgl3") },
                           { QStringLiteral("version"), QStringLiteral("3.3.1") },
                           { QStringLiteral("libraries"),
                             QJsonArray{ QJsonObject{
                                 { QStringLiteral("name"), QStringLiteral("org.lwjgl:lwjgl:3.3.1") },
                                 { QStringLiteral("downloads"),
                                   QJsonObject{ { QStringLiteral("artifact"),
                                                  QJsonObject{ { QStringLiteral("url"),
                                                                 QStringLiteral("https://libraries.minecraft.net/org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1.jar") },
                                                               { QStringLiteral("sha1"), sha1Hex(lwjglBytes) },
                                                               { QStringLiteral("size"), lwjglBytes.size() } } } } } } } } });

    const QDir instance(data.filePath(QStringLiteral("instances/test")));
    QVERIFY(QDir().mkpath(instance.path()));
    writeFile(instance.filePath(QStringLiteral("mmc-pack.json")),
              QJsonDocument(QJsonObject{
                                { QStringLiteral("components"),
                                  QJsonArray{ QJsonObject{ { QStringLiteral("uid"), QStringLiteral("org.lwjgl3") },
                                                           { QStringLiteral("version"), QStringLiteral("3.3.1") } },
                                              QJsonObject{ { QStringLiteral("uid"), QStringLiteral("net.minecraft") },
                                                           { QStringLiteral("version"), QStringLiteral("1.20.1") } } } } })
                  .toJson());
    writeFile(instance.filePath(QStringLiteral("instance.cfg")), QByteArrayLiteral("[General]\nname=Test\n"));

    QString error;
    const auto missing = missingInstanceFiles(data.path(), instance.path(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    // The present library is not asked for again; the wrong one, the absent one, the graphics
    // library and the game's own jar are.
    QStringList targets;
    for (const auto& request : missing)
        targets.append(request.targetPath);
    QVERIFY2(missing.size() == 4, qPrintable(targets.join(QStringLiteral(" | "))));
    QVERIFY(!targets.contains(data.filePath(QStringLiteral("libraries/com/example/present/1.0/present-1.0.jar"))));
    QVERIFY(targets.contains(data.filePath(QStringLiteral("libraries/com/example/wrong/1.0/wrong-1.0.jar"))));
    QVERIFY(targets.contains(data.filePath(QStringLiteral("libraries/com/example/absent/1.0/absent-1.0.jar"))));
    QVERIFY(targets.contains(data.filePath(QStringLiteral("libraries/org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1.jar"))));
    QVERIFY(targets.contains(data.filePath(QStringLiteral("libraries/com/mojang/minecraft/1.20.1/minecraft-1.20.1-client.jar"))));

    for (const auto& request : missing) {
        QVERIFY(request.url.isValid());
        QVERIFY(!request.url.isEmpty());
    }
}

void KomorebiInstanceFilesTest::reportsAnInstanceWithoutManifests()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    QVERIFY(missingInstanceFiles(temp.path(), temp.filePath(QStringLiteral("instances/none")), &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void KomorebiInstanceFilesTest::asksForNativeClassifiers()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QDir data(temp.path());

    const QByteArray nativeBytes = QByteArrayLiteral("the graphics library natives");
    const QByteArray derivedBytes = QByteArrayLiteral("a natives jar named after its classifier");

    // The missing-file list asks for the files this machine would run, so the fixture names the
    // platform it is being read on: CI builds Linux and macOS as well, and a manifest that only
    // spelled out "windows" would ask for nothing there.
    const auto platform = Komorebi::Core::Platform::current();
    const QString bits = platform.arch == QLatin1String("x86") ? QStringLiteral("32") : QStringLiteral("64");
    const QString classifierName = QStringLiteral("natives-%1").arg(bits);
    const QJsonObject natives{ { platform.os, QStringLiteral("natives-${arch}") } };

    // The two shapes the manifests use: one names the file, one only names the classifier.
    const QString withPathRelative =
        QStringLiteral("org/lwjgl/lwjgl/lwjgl-platform/2.9.4/lwjgl-platform-2.9.4-%1.jar").arg(classifierName);
    const QJsonObject withPathLibrary{
        { QStringLiteral("name"), QStringLiteral("org.lwjgl.lwjgl:lwjgl-platform:2.9.4") },
        { QStringLiteral("natives"), natives },
        { QStringLiteral("downloads"),
          QJsonObject{ { QStringLiteral("classifiers"),
                         QJsonObject{
                             { classifierName,
                               QJsonObject{ { QStringLiteral("path"), withPathRelative },
                                            { QStringLiteral("sha1"), sha1Hex(nativeBytes) },
                                            { QStringLiteral("size"), nativeBytes.size() } } } } } } }
    };
    const QJsonObject derivedLibrary{
        { QStringLiteral("name"), QStringLiteral("com.example:derived:1.0") },
        { QStringLiteral("natives"), natives },
        { QStringLiteral("downloads"),
          QJsonObject{ { QStringLiteral("classifiers"),
                         QJsonObject{
                             { classifierName,
                               QJsonObject{ { QStringLiteral("url"),
                                              QStringLiteral("https://example.invalid/derived-natives.jar") },
                                            { QStringLiteral("sha1"), sha1Hex(derivedBytes) },
                                            { QStringLiteral("size"), derivedBytes.size() } } } } } } }
    };

    writeJson(data.filePath(QStringLiteral("meta/net.minecraft/1.8.9.json")),
              QJsonObject{ { QStringLiteral("uid"), QStringLiteral("net.minecraft") },
                           { QStringLiteral("version"), QStringLiteral("1.8.9") },
                           { QStringLiteral("mainClass"), QStringLiteral("net.minecraft.client.main.Main") },
                           { QStringLiteral("libraries"), QJsonArray{ withPathLibrary, derivedLibrary } } });

    const QDir instance(data.filePath(QStringLiteral("instances/test")));
    QVERIFY(QDir().mkpath(instance.path()));
    writeFile(instance.filePath(QStringLiteral("mmc-pack.json")),
              QJsonDocument(QJsonObject{
                                { QStringLiteral("components"),
                                  QJsonArray{ QJsonObject{ { QStringLiteral("uid"), QStringLiteral("net.minecraft") },
                                                           { QStringLiteral("version"), QStringLiteral("1.8.9") } } } } })
                  .toJson());
    writeFile(instance.filePath(QStringLiteral("instance.cfg")), QByteArrayLiteral("[General]\nname=Test\n"));

    QString error;
    const auto missing = missingInstanceFiles(data.path(), instance.path(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    const QString withPath = data.filePath(QStringLiteral("libraries/") + withPathRelative);
    const QString derived =
        data.filePath(QStringLiteral("libraries/com/example/derived/1.0/derived-1.0-%1.jar").arg(classifierName));

    QStringList targets;
    for (const auto& request : missing)
        targets.append(request.targetPath);
    QVERIFY2(targets.contains(withPath), qPrintable(targets.join(QStringLiteral(" | "))));
    QVERIFY2(targets.contains(derived), qPrintable(targets.join(QStringLiteral(" | "))));

    for (const auto& request : missing) {
        if (request.targetPath == withPath) {
            // The classifier carries no address, so the file is the one the vanilla host keeps under its path.
            QCOMPARE(request.url.toString(), QStringLiteral("https://libraries.minecraft.net/") + withPathRelative);
            QCOMPARE(request.sha1, QByteArray::fromHex(sha1Hex(nativeBytes).toLatin1()));
        } else if (request.targetPath == derived) {
            QCOMPARE(request.url.toString(), QStringLiteral("https://example.invalid/derived-natives.jar"));
        }
    }
}

QTEST_GUILESS_MAIN(KomorebiInstanceFilesTest)

#include "KomorebiInstanceFiles_test.moc"
