// SPDX-License-Identifier: GPL-3.0-only
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "komorebi/core/asset/AssetIndex.h"
#include "komorebi/services/download/AssetFiles.h"

using namespace Komorebi::Core;
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

QJsonObject indexDocument(const QString& presentHash, qint64 presentSize, const QString& absentHash, qint64 absentSize)
{
    return QJsonObject{
        { QStringLiteral("objects"),
          QJsonObject{
              { QStringLiteral("minecraft/sounds/present.ogg"),
                QJsonObject{ { QStringLiteral("hash"), presentHash }, { QStringLiteral("size"), static_cast<double>(presentSize) } } },
              { QStringLiteral("minecraft/sounds/absent.ogg"),
                QJsonObject{ { QStringLiteral("hash"), absentHash }, { QStringLiteral("size"), static_cast<double>(absentSize) } } },
          } },
    };
}

QJsonObject manifestWithIndex(const QString& id, const QByteArray& indexBytes)
{
    return QJsonObject{
        { QStringLiteral("uid"), QStringLiteral("net.minecraft") },
        { QStringLiteral("version"), QStringLiteral("1.20.1") },
        { QStringLiteral("assetIndex"),
          QJsonObject{ { QStringLiteral("id"), id },
                       { QStringLiteral("sha1"), sha1Hex(indexBytes) },
                       { QStringLiteral("size"), indexBytes.size() },
                       { QStringLiteral("url"), QStringLiteral("https://piston-meta.mojang.com/v1/packages/abc/5.json") } } },
    };
}

}  // namespace

class KomorebiAssetFilesTest : public QObject {
    Q_OBJECT

   private slots:
    void parsesTheObjectList();
    void asksForTheIndexItDoesNotHave();
    void asksForTheObjectsThatAreMissing();

   private:
    static const QString kPresentHash;
    static const QString kAbsentHash;
};

const QString KomorebiAssetFilesTest::kPresentHash = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1");
const QString KomorebiAssetFilesTest::kAbsentHash = QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2");

void KomorebiAssetFilesTest::parsesTheObjectList()
{
    const auto document = indexDocument(kPresentHash, 10, kAbsentHash, 20);
    QString error;
    const auto index = parseAssetIndex(document, QStringLiteral("5"), &error);
    QVERIFY2(index.has_value(), qPrintable(error));
    QCOMPARE(index->id, QStringLiteral("5"));
    QCOMPARE(index->objects.size(), 2);
    QCOMPARE(index->objects.value(QStringLiteral("minecraft/sounds/present.ogg")).hash, kPresentHash);
    QCOMPARE(index->objects.value(QStringLiteral("minecraft/sounds/absent.ogg")).size, qint64(20));
}

void KomorebiAssetFilesTest::asksForTheIndexItDoesNotHave()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QByteArray indexBytes = QJsonDocument(indexDocument(kPresentHash, 10, kAbsentHash, 20)).toJson();

    const auto manifest = VersionJson::parse(manifestWithIndex(QStringLiteral("5"), indexBytes));
    QVERIFY(manifest.has_value());

    // Nothing on disk yet: the index file itself is one of the files that has to be fetched.
    auto missing = missingAssetIndexFile(*manifest, temp.path());
    QCOMPARE(missing.size(), 1);
    QCOMPARE(missing.first().targetPath, QDir(temp.path()).filePath(QStringLiteral("assets/indexes/5.json")));
    QCOMPARE(missing.first().sha1, QByteArray::fromHex(sha1Hex(indexBytes).toLatin1()));

    writeFile(missing.first().targetPath, indexBytes);
    QVERIFY(missingAssetIndexFile(*manifest, temp.path()).isEmpty());
}

void KomorebiAssetFilesTest::asksForTheObjectsThatAreMissing()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QByteArray presentBytes = QByteArrayLiteral("a sound that is present");
    const QString presentHash = sha1Hex(presentBytes);
    writeFile(QDir(temp.path()).filePath(QStringLiteral("assets/objects/") + presentHash.left(2) + QLatin1Char('/') + presentHash),
              presentBytes);

    const auto index = parseAssetIndex(indexDocument(presentHash, presentBytes.size(), kAbsentHash, 20), QStringLiteral("5"));
    QVERIFY(index.has_value());

    const auto missing = missingAssetObjects(*index, temp.path());
    QCOMPARE(missing.size(), 1);
    QCOMPARE(missing.first().targetPath,
             QDir(temp.path()).filePath(QStringLiteral("assets/objects/") + kAbsentHash.left(2) + QLatin1Char('/') + kAbsentHash));
    QCOMPARE(missing.first().url.toString(),
             QStringLiteral("https://resources.download.minecraft.net/") + kAbsentHash.left(2) + QLatin1Char('/') + kAbsentHash);
}

QTEST_GUILESS_MAIN(KomorebiAssetFilesTest)

#include "KomorebiAssetFiles_test.moc"
