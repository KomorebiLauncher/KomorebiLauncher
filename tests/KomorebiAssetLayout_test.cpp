// SPDX-License-Identifier: GPL-3.0-only
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "komorebi/core/asset/AssetLayout.h"

using namespace Komorebi::Core;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QJsonObject indexDocument(bool virtualLayout, bool mapToResources, const QString& soundHash, qint64 soundSize)
{
    QJsonObject document{
        { QStringLiteral("objects"),
          QJsonObject{
              { QStringLiteral("sound/hit.ogg"),
                QJsonObject{ { QStringLiteral("hash"), soundHash }, { QStringLiteral("size"), static_cast<double>(soundSize) } } },
          } },
    };
    if (virtualLayout)
        document.insert(QStringLiteral("virtual"), true);
    if (mapToResources)
        document.insert(QStringLiteral("map_to_resources"), true);
    return document;
}

}  // namespace

class KomorebiAssetLayoutTest : public QObject {
    Q_OBJECT

   private slots:
    void saysNothingForAModernIndex();
    void copiesIntoTheVirtualDirectory();
    void copiesIntoTheInstanceResources();
    void leavesFilesThatAreAlreadyRight();
};

void KomorebiAssetLayoutTest::saysNothingForAModernIndex()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto index = parseAssetIndex(indexDocument(false, false, QString(40, QLatin1Char('a')), 3), QStringLiteral("5"));
    QVERIFY(index.has_value());
    QVERIFY(legacyAssetDirectory(*index, temp.path(), temp.path()).isEmpty());
    QVERIFY(materialiseLegacyAssets(*index, temp.path(), temp.path()));
}

void KomorebiAssetLayoutTest::copiesIntoTheVirtualDirectory()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QByteArray sound = QByteArrayLiteral("ogg bytes");
    const QString hash = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1");
    writeFile(QDir(temp.path()).filePath(QStringLiteral("assets/objects/") + hash.left(2) + QLatin1Char('/') + hash), sound);
    writeFile(QDir(temp.path()).filePath(QStringLiteral("assets/indexes/legacy.json")), QByteArrayLiteral("{}"));

    const auto index = parseAssetIndex(indexDocument(true, false, hash, sound.size()), QStringLiteral("legacy"));
    QVERIFY(index.has_value());

    const QString destination = QDir(temp.path()).filePath(QStringLiteral("assets/virtual/legacy"));
    QCOMPARE(legacyAssetDirectory(*index, temp.path(), temp.path()), destination);

    QString error;
    QVERIFY2(materialiseLegacyAssets(*index, temp.path(), temp.path(), &error), qPrintable(error));
    QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("sound/hit.ogg"))), sound);
}

void KomorebiAssetLayoutTest::copiesIntoTheInstanceResources()
{
    QTemporaryDir data;
    QTemporaryDir instance;
    QVERIFY(data.isValid());
    QVERIFY(instance.isValid());

    const QByteArray sound = QByteArrayLiteral("older ogg bytes");
    const QString hash = QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2");
    writeFile(QDir(data.path()).filePath(QStringLiteral("assets/objects/") + hash.left(2) + QLatin1Char('/') + hash), sound);

    const auto index = parseAssetIndex(indexDocument(false, true, hash, sound.size()), QStringLiteral("pre-1.6"));
    QVERIFY(index.has_value());
    QCOMPARE(legacyAssetDirectory(*index, data.path(), instance.path()),
             QDir(instance.path()).filePath(QStringLiteral("resources")));

    QString error;
    QVERIFY2(materialiseLegacyAssets(*index, data.path(), instance.path(), &error), qPrintable(error));
    QCOMPARE(readFile(QDir(instance.path()).filePath(QStringLiteral("resources/sound/hit.ogg"))), sound);
}

void KomorebiAssetLayoutTest::leavesFilesThatAreAlreadyRight()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QByteArray sound = QByteArrayLiteral("already copied");
    const QString hash = QStringLiteral("ccccccccccccccccccccccccccccccccccccccc3");
    writeFile(QDir(temp.path()).filePath(QStringLiteral("assets/objects/") + hash.left(2) + QLatin1Char('/') + hash), sound);

    const auto index = parseAssetIndex(indexDocument(true, false, hash, sound.size()), QStringLiteral("legacy"));
    QVERIFY(index.has_value());
    const QString target = QDir(legacyAssetDirectory(*index, temp.path(), temp.path())).filePath(QStringLiteral("sound/hit.ogg"));
    writeFile(target, sound);

    const QDateTime before = QFileInfo(target).lastModified();
    QVERIFY(materialiseLegacyAssets(*index, temp.path(), temp.path()));
    QCOMPARE(QFileInfo(target).lastModified(), before);
}

QTEST_GUILESS_MAIN(KomorebiAssetLayoutTest)

#include "KomorebiAssetLayout_test.moc"
