// SPDX-License-Identifier: GPL-3.0-only
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "komorebi/core/version/ComponentSet.h"

using namespace Komorebi::Core;

class KomorebiComponentSetTest : public QObject {
    Q_OBJECT

   private slots:
    void loadsEveryComponentsManifest();
    void mergesGameFieldsAndAllLibraries();
    void reportsAMissingComponent();

   private:
    static void write(const QString& directory, const QString& relative, const QString& text);
};

void KomorebiComponentSetTest::write(const QString& directory, const QString& relative, const QString& text)
{
    const QString path = QDir(directory).filePath(relative);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(text.toUtf8());
    file.close();
}

void KomorebiComponentSetTest::loadsEveryComponentsManifest()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    write(temp.path(), QStringLiteral("net.minecraft/1.20.1.json"),
          QStringLiteral(R"({ "uid": "net.minecraft", "version": "1.20.1", "mainClass": "net.minecraft.client.main.Main" })"));
    write(temp.path(), QStringLiteral("org.lwjgl3/3.3.1.json"),
          QStringLiteral(R"({ "uid": "org.lwjgl3", "version": "3.3.1", "libraries": [ { "name": "org.lwjgl:lwjgl:3.3.1" } ] })"));

    const QList<ComponentRef> refs{ { QStringLiteral("net.minecraft"), QStringLiteral("1.20.1") },
                                    { QStringLiteral("org.lwjgl3"), QStringLiteral("3.3.1") } };
    const auto loaded = loadComponents(temp.path(), refs);
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->size(), 2);
    QCOMPARE(loaded->at(1).uid, QStringLiteral("org.lwjgl3"));
}

void KomorebiComponentSetTest::mergesGameFieldsAndAllLibraries()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    write(temp.path(), QStringLiteral("net.minecraft/26.4-snapshot-7.json"), QStringLiteral(R"({
        "uid": "net.minecraft",
        "version": "26.4-snapshot-7",
        "mainClass": "net.minecraft.client.main.Main",
        "compatibleJavaMajors": [ 25 ],
        "mainJar": { "name": "com.mojang:minecraft:26.4-snapshot-7:client" },
        "minecraftArguments": "--username ${auth_player_name}",
        "libraries": [ { "name": "com.mojang:brigadier:1.0.18" } ]
    })"));
    write(temp.path(), QStringLiteral("org.lwjgl3/3.4.3.json"), QStringLiteral(R"({
        "uid": "org.lwjgl3",
        "version": "3.4.3",
        "libraries": [ { "name": "org.lwjgl:lwjgl:3.4.3" } ]
    })"));

    const QList<ComponentRef> refs{ { QStringLiteral("org.lwjgl3"), QStringLiteral("3.4.3") },
                                    { QStringLiteral("net.minecraft"), QStringLiteral("26.4-snapshot-7") } };
    QString error;
    const auto loaded = loadComponents(temp.path(), refs, &error);
    QVERIFY2(loaded.has_value(), qPrintable(error));
    const auto merged = mergeComponents(*loaded, &error);
    QVERIFY2(merged.has_value(), qPrintable(error));

    QCOMPARE(merged->mainClass, QStringLiteral("net.minecraft.client.main.Main"));
    QCOMPARE(merged->javaMajor, 25);
    QCOMPARE(merged->mainJarName, QStringLiteral("com.mojang:minecraft:26.4-snapshot-7:client"));
    QCOMPARE(merged->libraries.size(), 2);
    // Components are folded in the order the instance lists them, so the graphics library is first.
    QCOMPARE(merged->libraries.at(0).name, QStringLiteral("org.lwjgl:lwjgl:3.4.3"));
    QCOMPARE(merged->libraries.at(1).name, QStringLiteral("com.mojang:brigadier:1.0.18"));
    QCOMPARE(merged->legacyArguments.first(), QStringLiteral("--username"));
}

void KomorebiComponentSetTest::reportsAMissingComponent()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    write(temp.path(), QStringLiteral("net.minecraft/1.20.1.json"),
          QStringLiteral(R"({ "uid": "net.minecraft", "version": "1.20.1" })"));

    const QList<ComponentRef> refs{ { QStringLiteral("net.minecraft"), QStringLiteral("1.20.1") },
                                    { QStringLiteral("org.lwjgl3"), QStringLiteral("3.3.1") } };
    QString error;
    QVERIFY(!loadComponents(temp.path(), refs, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("org.lwjgl3")));
}

QTEST_GUILESS_MAIN(KomorebiComponentSetTest)

#include "KomorebiComponentSet_test.moc"
