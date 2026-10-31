// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "FileSystem.h"
#include "java/JavaUtils.h"
#include "java/LocalJavaScanTask.h"

class LocalJavaScanTest : public QObject {
    Q_OBJECT
   private:
    /** Creates <dir>/bin/<javaExecutable> and returns the full path of the executable. */
    static auto makeRuntime(const QString& dir) -> QString
    {
        QDir().mkpath(FS::PathCombine(dir, QStringLiteral("bin")));
        auto executable = FS::PathCombine(dir, QStringLiteral("bin"), JavaUtils::javaExecutable);
        QFile file(executable);
        if (file.open(QIODevice::WriteOnly)) {
            file.write("not a real java");
            file.close();
        }
        return QDir(executable).absolutePath();
    }

   private slots:
    void test_FindsRuntimesInBothLayouts()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        auto root = QDir(tmp.path()).absolutePath();

        auto inRuntime = makeRuntime(FS::PathCombine(root, QStringLiteral("runtime"), QStringLiteral("java-runtime-gamma")));
        auto inDotMinecraft = makeRuntime(FS::PathCombine(root, QStringLiteral(".minecraft"), QStringLiteral("runtime"), QStringLiteral("jre-8")));

        auto found = Java::LocalJavaScanTask::findCandidates(root);
        QCOMPARE(found.size(), 2);
        QVERIFY(found.contains(inRuntime));
        QVERIFY(found.contains(inDotMinecraft));
    }

    void test_IgnoresIncompleteDirectories()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        auto root = QDir(tmp.path()).absolutePath();

        // A runtime folder without bin/<javaExecutable> is not a candidate.
        QDir().mkpath(FS::PathCombine(root, QStringLiteral("runtime"), QStringLiteral("empty"), QStringLiteral("lib")));
        // Nor is a stray file at the top of a runtime directory.
        QDir().mkpath(FS::PathCombine(root, QStringLiteral("runtime")));
        QFile readme(FS::PathCombine(root, QStringLiteral("runtime"), QStringLiteral("readme.txt")));
        QVERIFY(readme.open(QIODevice::WriteOnly));
        readme.close();

        QVERIFY(Java::LocalJavaScanTask::findCandidates(root).isEmpty());
        QVERIFY(Java::LocalJavaScanTask::findCandidates(FS::PathCombine(root, QStringLiteral("does-not-exist"))).isEmpty());
    }

    void test_ReportsNothingForAnEmptyTree()
    {
        // A directory that exists but holds no runtime must yield an empty result rather than an error.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QDir().mkpath(FS::PathCombine(tmp.path(), QStringLiteral("runtime")));
        QVERIFY(Java::LocalJavaScanTask::findCandidates(tmp.path()).isEmpty());
    }

    void test_ResultIsSortedAndDeduplicated()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        auto root = QDir(tmp.path()).absolutePath();

        makeRuntime(FS::PathCombine(root, QStringLiteral("runtime"), QStringLiteral("z-last")));
        makeRuntime(FS::PathCombine(root, QStringLiteral("runtime"), QStringLiteral("a-first")));
        // The same executable can be reachable through both layouts only by a symlink, but a
        // duplicate must never be reported twice.
        makeRuntime(FS::PathCombine(root, QStringLiteral("runtime"), QStringLiteral("a-first")));

        auto found = Java::LocalJavaScanTask::findCandidates(root);
        QCOMPARE(found.size(), 2);
        auto sorted = found;
        std::sort(sorted.begin(), sorted.end());
        QCOMPARE(found, sorted);
    }
};

QTEST_GUILESS_MAIN(LocalJavaScanTest)

#include "LocalJavaScan_test.moc"
