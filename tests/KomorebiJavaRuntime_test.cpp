// SPDX-License-Identifier: GPL-3.0-only
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "komorebi/services/java/JavaRuntime.h"

using namespace Komorebi::Services;

class KomorebiJavaRuntimeTest : public QObject {
    Q_OBJECT

   private slots:
    void majorFromVersionNormalizesOldAndNew();
    void parseReleaseReadsTheRuntimeFile();
    void parseReleaseRejectsTextWithoutAVersion();
    void choosePrefersExactThenNewerThenOlder();
    void scanFindsTheMojangLayout();
};

void KomorebiJavaRuntimeTest::majorFromVersionNormalizesOldAndNew()
{
    QCOMPARE(majorFromVersion(QStringLiteral("1.8.0_202")), 8);
    QCOMPARE(majorFromVersion(QStringLiteral("17.0.9")), 17);
    QCOMPARE(majorFromVersion(QStringLiteral("21")), 21);
    QCOMPARE(majorFromVersion(QStringLiteral("jdk-17.0.2")), 17);
    QCOMPARE(majorFromVersion(QString()), 0);
    QCOMPARE(majorFromVersion(QStringLiteral("nonsense")), 0);
}

void KomorebiJavaRuntimeTest::parseReleaseReadsTheRuntimeFile()
{
    const QString text = QStringLiteral(
        "JAVA_VERSION=\"17.0.9\"\n"
        "OS_ARCH=\"x86_64\"\n"
        "IMPLEMENTOR=\"Eclipse Adoptium\"\n"
        "OS_NAME=\"Windows\"\n");

    const auto runtime = parseRelease(text, QStringLiteral("C:/jdk-17"));
    QVERIFY(runtime.has_value());
    QCOMPARE(runtime->version, QStringLiteral("17.0.9"));
    QCOMPARE(runtime->major, 17);
    QCOMPARE(runtime->architecture, QStringLiteral("x86_64"));
    QCOMPARE(runtime->vendor, QStringLiteral("Eclipse Adoptium"));
    QCOMPARE(runtime->home, QStringLiteral("C:/jdk-17"));
    QCOMPARE(runtime->path, executableIn(runtime->home));
    QVERIFY(runtime->isValid());
}

void KomorebiJavaRuntimeTest::parseReleaseRejectsTextWithoutAVersion()
{
    QVERIFY(!parseRelease(QStringLiteral("OS_ARCH=\"x86_64\"\n"), QStringLiteral("C:/jdk")).has_value());
    QVERIFY(!parseRelease(QString(), QStringLiteral("C:/jdk")).has_value());
}

void KomorebiJavaRuntimeTest::choosePrefersExactThenNewerThenOlder()
{
    const auto runtime = [](const QString& version, int major) {
        JavaRuntime value;
        value.path = QStringLiteral("C:/jdk-%1/bin/java").arg(major);
        value.home = QStringLiteral("C:/jdk-%1").arg(major);
        value.version = version;
        value.major = major;
        return value;
    };

    const QList<JavaRuntime> all{ runtime(QStringLiteral("8.0.402"), 8), runtime(QStringLiteral("17.0.9"), 17),
                                  runtime(QStringLiteral("21.0.1"), 21) };

    QCOMPARE(choose(all, 17)->major, 17);
    QCOMPARE(choose(all, 16)->major, 17);   // the smallest major that can still run it
    QCOMPARE(choose(all, 22)->major, 21);   // nothing above, so the newest below
    QVERIFY(!choose(all, 0).has_value());

    const QList<JavaRuntime> none{};
    QVERIFY(!choose(none, 17).has_value());

    // An entry the scanner could not identify is not a candidate.
    JavaRuntime unknown;
    unknown.path = QStringLiteral("C:/jdk-unknown/bin/java");
    QVERIFY(!choose({ unknown }, 17).has_value());
}

void KomorebiJavaRuntimeTest::scanFindsTheMojangLayout()
{
    QTemporaryDir dataRoot;
    QVERIFY(dataRoot.isValid());

    const QString home = QDir(dataRoot.path()).filePath(QStringLiteral("java/java-runtime-gamma/windows-x64/java-runtime-gamma"));
    QVERIFY(QDir().mkpath(QDir(home).filePath(QStringLiteral("bin"))));
    QFile executable(executableIn(home));
    QVERIFY(executable.open(QIODevice::WriteOnly));
    executable.close();
    QFile release(QDir(home).filePath(QStringLiteral("release")));
    QVERIFY(release.open(QIODevice::WriteOnly));
    release.write("JAVA_VERSION=\"17.0.9\"\nOS_ARCH=\"x86_64\"\n");
    release.close();

    const auto runtimes = scanMojangRuntimes(dataRoot.path());
    QCOMPARE(runtimes.size(), 1);
    QCOMPARE(runtimes.first().major, 17);
    QCOMPARE(runtimes.first().version, QStringLiteral("17.0.9"));
    QCOMPARE(runtimes.first().path, executableIn(home));
}

QTEST_GUILESS_MAIN(KomorebiJavaRuntimeTest)

#include "KomorebiJavaRuntime_test.moc"
