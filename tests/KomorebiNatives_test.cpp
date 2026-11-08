// SPDX-License-Identifier: GPL-3.0-only
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "komorebi/core/launch/Natives.h"

using namespace Komorebi::Core;

namespace {

quint32 crc32Of(const QByteArray& data)
{
    quint32 crc = 0xFFFFFFFF;
    for (const char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
    }
    return ~crc;
}

void appendLe16(QByteArray& out, quint16 value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

void appendLe32(QByteArray& out, quint32 value)
{
    for (int shift = 0; shift < 32; shift += 8)
        out.append(static_cast<char>((value >> shift) & 0xFF));
}

/// A stored (uncompressed) zip, which is all a natives jar needs to be for the extractor.
QByteArray buildZip(const QList<QPair<QString, QByteArray>>& files)
{
    QByteArray out;
    QByteArray central;
    for (const auto& entry : files) {
        const QString name = entry.first;
        const QByteArray body = entry.second;
        const QByteArray encoded = name.toUtf8();
        const quint32 crc = crc32Of(body);

        const quint32 offset = static_cast<quint32>(out.size());
        appendLe32(out, 0x04034b50);
        appendLe16(out, 20);
        appendLe16(out, 0);
        appendLe16(out, 0);  // stored
        appendLe16(out, 0);
        appendLe16(out, 0);
        appendLe32(out, crc);
        appendLe32(out, static_cast<quint32>(body.size()));
        appendLe32(out, static_cast<quint32>(body.size()));
        appendLe16(out, static_cast<quint16>(encoded.size()));
        appendLe16(out, 0);
        out.append(encoded);
        out.append(body);

        appendLe32(central, 0x02014b50);
        appendLe16(central, 20);
        appendLe16(central, 20);
        appendLe16(central, 0);
        appendLe16(central, 0);
        appendLe16(central, 0);
        appendLe16(central, 0);
        appendLe32(central, crc);
        appendLe32(central, static_cast<quint32>(body.size()));
        appendLe32(central, static_cast<quint32>(body.size()));
        appendLe16(central, static_cast<quint16>(encoded.size()));
        appendLe16(central, 0);
        appendLe16(central, 0);
        appendLe16(central, 0);
        appendLe16(central, 0);
        appendLe32(central, 0);
        appendLe32(central, offset);
        central.append(encoded);
    }

    const quint32 centralOffset = static_cast<quint32>(out.size());
    out.append(central);
    appendLe32(out, 0x06054b50);
    appendLe16(out, 0);
    appendLe16(out, 0);
    appendLe16(out, static_cast<quint16>(files.size()));
    appendLe16(out, static_cast<quint16>(files.size()));
    appendLe32(out, static_cast<quint32>(central.size()));
    appendLe32(out, centralOffset);
    appendLe16(out, 0);
    return out;
}

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

QJsonObject libraryEntry(const QString& name, const QString& url)
{
    return QJsonObject{
        { QStringLiteral("name"), name },
        { QStringLiteral("downloads"), QJsonObject{ { QStringLiteral("artifact"), QJsonObject{ { QStringLiteral("url"), url } } } } },
    };
}

QJsonObject nativesManifest()
{
    // Written as objects rather than a raw string: moc's parser loses the class in this file when a
    // raw string sits above it, and a test that cannot be moc'd cannot be a test.
    auto windowsNatives = libraryEntry(QStringLiteral("org.lwjgl:lwjgl-natives-windows:3.3.1"),
                                       QStringLiteral("https://libraries.minecraft.net/org/lwjgl/lwjgl/3.3.1/"
                                                      "lwjgl-3.3.1-natives-windows.jar"));
    windowsNatives.insert(QStringLiteral("rules"),
                          QJsonArray{ QJsonObject{ { QStringLiteral("action"), QStringLiteral("allow") },
                                                   { QStringLiteral("os"),
                                                     QJsonObject{ { QStringLiteral("name"), QStringLiteral("windows") } } } } });

    return QJsonObject{
        { QStringLiteral("uid"), QStringLiteral("org.lwjgl3") },
        { QStringLiteral("version"), QStringLiteral("3.3.1") },
        { QStringLiteral("mainClass"), QStringLiteral("net.minecraft.client.main.Main") },
        { QStringLiteral("libraries"),
          QJsonArray{ windowsNatives,
                      libraryEntry(QStringLiteral("org.lwjgl:lwjgl:3.3.1"),
                                   QStringLiteral("https://libraries.minecraft.net/org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1.jar")) } },
    };
}
}  // namespace

class KomorebiNativesTest : public QObject {
    Q_OBJECT

   private slots:
    void findsTheJarsForThePlatform();
    void unpacksThemWithoutTheirMetadata();
    void leavesTheDirectoryAloneTheSecondTime();
};

void KomorebiNativesTest::findsTheJarsForThePlatform()
{
    const auto manifest = VersionJson::parse(nativesManifest());
    QVERIFY(manifest.has_value());

    QTemporaryDir libraries;
    QVERIFY(libraries.isValid());
    const auto jars = nativeJars(*manifest, libraries.path(), Platform{ QStringLiteral("windows"), QStringLiteral("x86_64") });
    QCOMPARE(jars.size(), 1);
    QCOMPARE(jars.first().path,
             QDir(libraries.path()).filePath(QStringLiteral("org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows.jar")));

    // The same version on another system lists no natives.
    QVERIFY(nativeJars(*manifest, libraries.path(), Platform{ QStringLiteral("linux"), QStringLiteral("x86_64") }).isEmpty());
}

void KomorebiNativesTest::unpacksThemWithoutTheirMetadata()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QDir libraries(temp.path());
    const QString jarPath = libraries.filePath(QStringLiteral("org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows.jar"));
    writeFile(jarPath, buildZip({ { QStringLiteral("lwjgl.dll"), QByteArrayLiteral("the dll") },
                                  { QStringLiteral("META-INF/MANIFEST.MF"), QByteArrayLiteral("Manifest-Version: 1.0") },
                                  { QStringLiteral("nested/readme.txt"), QByteArrayLiteral("nested file") } }));

    const auto manifest = VersionJson::parse(nativesManifest());
    QVERIFY(manifest.has_value());
    const auto jars = nativeJars(*manifest, libraries.path(), Platform{ QStringLiteral("windows"), QStringLiteral("x86_64") });
    QCOMPARE(jars.size(), 1);

    const QString natives = temp.filePath(QStringLiteral("natives"));
    QString error;
    QVERIFY2(extractNatives(jars, natives, &error), qPrintable(error));
    QVERIFY(QFile::exists(QDir(natives).filePath(QStringLiteral("lwjgl.dll"))));
    QVERIFY(QFile::exists(QDir(natives).filePath(QStringLiteral("nested/readme.txt"))));
    QVERIFY(!QFile::exists(QDir(natives).filePath(QStringLiteral("META-INF/MANIFEST.MF"))));
}

void KomorebiNativesTest::leavesTheDirectoryAloneTheSecondTime()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QDir libraries(temp.path());
    writeFile(libraries.filePath(QStringLiteral("org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows.jar")),
              buildZip({ { QStringLiteral("lwjgl.dll"), QByteArrayLiteral("the dll") } }));

    const auto manifest = VersionJson::parse(nativesManifest());
    QVERIFY(manifest.has_value());
    const auto jars = nativeJars(*manifest, libraries.path(), Platform{ QStringLiteral("windows"), QStringLiteral("x86_64") });

    const QString natives = temp.filePath(QStringLiteral("natives"));
    QVERIFY(extractNatives(jars, natives));
    QVERIFY(QFile::exists(QDir(natives).filePath(QStringLiteral(".komorebi-natives.json"))));

    // Removing the unpacked file proves the second call did not rewrite it.
    QVERIFY(QFile::remove(QDir(natives).filePath(QStringLiteral("lwjgl.dll"))));
    QVERIFY(extractNatives(jars, natives));
    QVERIFY(!QFile::exists(QDir(natives).filePath(QStringLiteral("lwjgl.dll"))));
}

QTEST_GUILESS_MAIN(KomorebiNativesTest)

#include "KomorebiNatives_test.moc"
