// SPDX-License-Identifier: GPL-3.0-only
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "komorebi/core/instance/InstanceFolder.h"

using namespace Komorebi::Core;

class KomorebiInstanceFolderTest : public QObject {
    Q_OBJECT

   private slots:
    void readsTheComponentList();
    void readsTheNameFromTheConfig();
    void reportsAMissingPack();
    void reportsAManifestThatIsNotOne();

   private:
    static void write(const QString& directory, const QString& name, const QString& text);
};

void KomorebiInstanceFolderTest::write(const QString& directory, const QString& name, const QString& text)
{
    QFile file(QDir(directory).filePath(name));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(text.toUtf8());
    file.close();
}

void KomorebiInstanceFolderTest::readsTheComponentList()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    write(temp.path(), QStringLiteral("mmc-pack.json"), QStringLiteral(R"({
        "formatVersion": 1,
        "components": [
            { "uid": "org.lwjgl3", "version": "3.4.3" },
            { "uid": "net.minecraft", "version": "26.4-snapshot-7" },
            { "uid": "net.minecraftforge", "version": "47.4.0" }
        ]
    })"));

    const auto folder = InstanceFolder::read(temp.path());
    QVERIFY(folder.has_value());
    QCOMPARE(folder->components.size(), 3);
    QCOMPARE(folder->minecraftVersion(), QStringLiteral("26.4-snapshot-7"));
    QCOMPARE(folder->loaderUid(), QStringLiteral("net.minecraftforge"));
    QVERIFY(folder->component(QStringLiteral("org.lwjgl3")).has_value());
}

void KomorebiInstanceFolderTest::readsTheNameFromTheConfig()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    write(temp.path(), QStringLiteral("mmc-pack.json"),
          QStringLiteral(R"({ "components": [ { "uid": "net.minecraft", "version": "1.20.1" } ] })"));
    write(temp.path(), QStringLiteral("instance.cfg"),
          QStringLiteral("[General]\nInstanceType=OneSix\nname=My Instance\niconKey=default\n"));

    const auto folder = InstanceFolder::read(temp.path());
    QVERIFY(folder.has_value());
    QCOMPARE(folder->name, QStringLiteral("My Instance"));
}

void KomorebiInstanceFolderTest::reportsAMissingPack()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    QVERIFY(!InstanceFolder::read(temp.path(), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("mmc-pack.json")));
}

void KomorebiInstanceFolderTest::reportsAManifestThatIsNotOne()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    write(temp.path(), QStringLiteral("mmc-pack.json"), QStringLiteral("{ not json"));
    QString error;
    QVERIFY(!InstanceFolder::read(temp.path(), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("mmc-pack.json")));
}

QTEST_GUILESS_MAIN(KomorebiInstanceFolderTest)

#include "KomorebiInstanceFolder_test.moc"
