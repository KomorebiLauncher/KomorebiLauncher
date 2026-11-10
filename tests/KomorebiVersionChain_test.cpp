// SPDX-License-Identifier: GPL-3.0-only
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "komorebi/core/version/VersionChain.h"

using namespace Komorebi::Core;

class KomorebiVersionChainTest : public QObject {
    Q_OBJECT

   private slots:
    void loadsChildThenParent();
    void followsInheritanceAndMerges();
    void reportsAMissingVersion();
    void reportsAMissingParent();
    void reportsALoop();
    void refusesAParentThatIsAPath();
    void reportsAManifestThatIsNotOne();
};

namespace {

void writeManifest(const QString& versionsRoot, const QString& id, const QString& json)
{
    const QString directory = QDir(versionsRoot).filePath(id);
    QDir().mkpath(directory);
    QFile file(QDir(directory).filePath(id + QStringLiteral(".json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(json.toUtf8());
    file.close();
}

}  // namespace

void KomorebiVersionChainTest::loadsChildThenParent()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();

    writeManifest(root, QStringLiteral("parent"),
                  QStringLiteral(R"({ "id": "parent", "mainClass": "a.B", "libraries": [ { "name": "x:y:1" } ] })"));
    writeManifest(root, QStringLiteral("child"), QStringLiteral(R"({ "id": "child", "inheritsFrom": "parent" })"));

    const auto chain = loadVersionChain(root, QStringLiteral("child"));
    QVERIFY(chain.has_value());
    QCOMPARE(chain->size(), 2);
    QCOMPARE(chain->at(0).id, QStringLiteral("child"));
    QCOMPARE(chain->at(1).id, QStringLiteral("parent"));
}

void KomorebiVersionChainTest::followsInheritanceAndMerges()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();

    writeManifest(root, QStringLiteral("base"),
                  QStringLiteral(R"({ "id": "base", "mainClass": "net.minecraft.client.main.Main",
                                      "libraries": [ { "name": "base:lib:1" } ] })"));
    writeManifest(root, QStringLiteral("loader"),
                  QStringLiteral(R"({ "id": "loader", "inheritsFrom": "base", "mainClass": "loader.Main",
                                      "libraries": [ { "name": "loader:lib:1" } ] })"));

    const auto chain = loadVersionChain(root, QStringLiteral("loader"));
    QVERIFY(chain.has_value());
    const auto merged = mergeChain(*chain);
    QVERIFY(merged.has_value());
    QCOMPARE(merged->mainClass, QStringLiteral("loader.Main"));
    QCOMPARE(merged->libraries.size(), 2);
}

void KomorebiVersionChainTest::reportsAMissingVersion()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    QVERIFY(!loadVersionChain(temp.path(), QStringLiteral("nope"), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("nope")));
}

void KomorebiVersionChainTest::reportsAMissingParent()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    writeManifest(temp.path(), QStringLiteral("child"), QStringLiteral(R"({ "id": "child", "inheritsFrom": "absent" })"));

    QString error;
    QVERIFY(!loadVersionChain(temp.path(), QStringLiteral("child"), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("absent")));
}

void KomorebiVersionChainTest::reportsALoop()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    writeManifest(temp.path(), QStringLiteral("a"), QStringLiteral(R"({ "id": "a", "inheritsFrom": "b" })"));
    writeManifest(temp.path(), QStringLiteral("b"), QStringLiteral(R"({ "id": "b", "inheritsFrom": "a" })"));

    QString error;
    QVERIFY(!loadVersionChain(temp.path(), QStringLiteral("a"), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("inherits")));
}

void KomorebiVersionChainTest::refusesAParentThatIsAPath()
{
    // inheritsFrom names a directory below versions/, and an inherited id comes out of a manifest a
    // loader or a pack wrote; "../../" there would read a file outside the launcher's versions folder.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    writeManifest(temp.path(), QStringLiteral("child"),
                  QStringLiteral(R"({ "id": "child", "inheritsFrom": "../../../secret" })"));

    QString error;
    QVERIFY(!loadVersionChain(temp.path(), QStringLiteral("child"), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("version id")));
}

void KomorebiVersionChainTest::reportsAManifestThatIsNotOne()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    writeManifest(temp.path(), QStringLiteral("broken"), QStringLiteral("{ this is not json "));

    QString error;
    QVERIFY(!loadVersionChain(temp.path(), QStringLiteral("broken"), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("broken")));
}

QTEST_GUILESS_MAIN(KomorebiVersionChainTest)

#include "KomorebiVersionChain_test.moc"
