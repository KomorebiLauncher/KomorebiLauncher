// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QTest>

#include "java/JavaSelection.h"

static auto makeInstall(const QString& path, int major) -> JavaInstallPtr
{
    auto install = std::make_shared<JavaInstall>();
    install->path = path;
    install->id = JavaVersion(major, 0, 0);
    install->is_64bit = true;
    return install;
}

class JavaSelectionTest : public QObject {
    Q_OBJECT

   private slots:
    void test_ParseVersionList_data()
    {
        QTest::addColumn<QString>("spec");
        QTest::addColumn<QList<int>>("expected");

        QTest::newRow("empty means unrestricted") << QString() << QList<int>{};
        QTest::newRow("single") << QStringLiteral("8") << QList<int>{ 8 };
        QTest::newRow("range") << QStringLiteral("8-11") << QList<int>{ 8, 9, 10, 11 };
        QTest::newRow("reversed range") << QStringLiteral("11-8") << QList<int>{ 8, 9, 10, 11 };
        QTest::newRow("mixed with spaces") << QStringLiteral("8, 11, 17-19") << QList<int>{ 8, 11, 17, 18, 19 };
        QTest::newRow("duplicates collapse") << QStringLiteral("8,8,8-9") << QList<int>{ 8, 9 };
        QTest::newRow("garbage is ignored") << QStringLiteral("latest") << QList<int>{};
        QTest::newRow("garbage among good entries") << QStringLiteral("8, latest") << QList<int>{ 8 };
        QTest::newRow("absurd span is clamped") << QStringLiteral("1-9999") << [] {
            QList<int> out;
            for (int i = 1; i <= 65; ++i)
                out << i;
            return out;
        }();
    }

    void test_ParseVersionList()
    {
        QFETCH(QString, spec);
        QFETCH(QList<int>, expected);

        auto parsed = Java::parseVersionList(spec);
        std::sort(parsed.begin(), parsed.end());
        QCOMPARE(parsed, expected);
    }

    void test_IsVersionAllowed()
    {
        QVERIFY(Java::isVersionAllowed(8, {}));
        QVERIFY(Java::isVersionAllowed(8, { 8, 17 }));
        QVERIFY(!Java::isVersionAllowed(11, { 8, 17 }));
    }

    void test_PickInstall()
    {
        QList<JavaInstallPtr> installs{ makeInstall(QStringLiteral("jre8"), 8), makeInstall(QStringLiteral("jre17"), 17),
                                        makeInstall(QStringLiteral("jre21"), 21) };

        // Without a restriction the first compatible installation wins, as before.
        QCOMPARE(Java::pickInstall(installs, { 8, 17 }, {})->path, QStringLiteral("jre8"));
        // A restriction must be able to skip it.
        QCOMPARE(Java::pickInstall(installs, { 8, 17 }, { 17 })->path, QStringLiteral("jre17"));
        // Nothing compatible at all.
        QVERIFY(Java::pickInstall(installs, { 8 }, { 21 }) == nullptr);
        QVERIFY(Java::pickInstall(installs, { 11 }, {}) == nullptr);
        QVERIFY(Java::pickInstall({}, { 8 }, {}) == nullptr);
        // A null entry in the list must not be dereferenced.
        QList<JavaInstallPtr> withNull{ nullptr, makeInstall(QStringLiteral("jre8"), 8) };
        QCOMPARE(Java::pickInstall(withNull, { 8 }, {})->path, QStringLiteral("jre8"));
    }
};

QTEST_GUILESS_MAIN(JavaSelectionTest)

#include "JavaSelection_test.moc"
