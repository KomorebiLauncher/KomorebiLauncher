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

#include "net/MirrorPolicy.h"

using Kind = Net::MirrorPolicyKind;

static QStringList toStrings(const std::vector<QUrl>& urls)
{
    QStringList out;
    for (const auto& url : urls)
        out << url.toString();
    return out;
}

class MirrorPolicyTest : public QObject {
    Q_OBJECT

   private slots:
    void test_Rules_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<int>("kind");
        QTest::addColumn<QStringList>("expected");

        const auto base = QStringLiteral("https://bmclapi2.bangbang93.com");

        // A jar hosted by piston-data keeps its path and lands at the mirror root.
        QTest::newRow("mojang object, official first")
            << QStringLiteral("https://piston-data.mojang.com/v1/objects/a7e5a6/client.jar") << static_cast<int>(Kind::OfficialThenMirror)
            << QStringList{ QStringLiteral("https://piston-data.mojang.com/v1/objects/a7e5a6/client.jar"),
                           base + QStringLiteral("/v1/objects/a7e5a6/client.jar") };

        QTest::newRow("mojang meta, mirror first") << QStringLiteral("https://piston-meta.mojang.com/v1/packages/e61d8c/19.json")
                                                   << static_cast<int>(Kind::MirrorThenOfficial)
                                                   << QStringList{ base + QStringLiteral("/v1/packages/e61d8c/19.json"),
                                                                  QStringLiteral("https://piston-meta.mojang.com/v1/packages/e61d8c/19.json") };

        // Asset objects go under /assets, not the root.
        QTest::newRow("asset object") << QStringLiteral("https://resources.download.minecraft.net/af/af96f5")
                                      << static_cast<int>(Kind::MirrorOnly)
                                      << QStringList{ base + QStringLiteral("/assets/af/af96f5") };

        // Legacy manifests used plain http; the host still matches.
        QTest::newRow("legacy http asset") << QStringLiteral("http://resources.download.minecraft.net/19/1920de")
                                           << static_cast<int>(Kind::MirrorOnly)
                                           << QStringList{ base + QStringLiteral("/assets/19/1920de") };

        // Maven libraries get both prefixes, /maven before /libraries.
        QTest::newRow("maven library") << QStringLiteral("https://libraries.minecraft.net/com/mojang/patchy/2.2.10/patchy-2.2.10.jar")
                                       << static_cast<int>(Kind::MirrorOnly)
                                       << QStringList{ base + QStringLiteral("/maven/com/mojang/patchy/2.2.10/patchy-2.2.10.jar"),
                                                      base + QStringLiteral("/libraries/com/mojang/patchy/2.2.10/patchy-2.2.10.jar") };

        // Anything else must not be touched, especially API hosts that carry keys.
        QTest::newRow("modrinth untouched") << QStringLiteral("https://cdn.modrinth.com/data/P7dR8mSH/versions/x/fabric-api.jar")
                                            << static_cast<int>(Kind::MirrorOnly)
                                            << QStringList{ QStringLiteral("https://cdn.modrinth.com/data/P7dR8mSH/versions/x/fabric-api.jar") };

        QTest::newRow("curseforge untouched") << QStringLiteral("https://edge.forgecdn.net/files/5532/710/jei.jar")
                                              << static_cast<int>(Kind::MirrorThenOfficial)
                                              << QStringList{ QStringLiteral("https://edge.forgecdn.net/files/5532/710/jei.jar") };

        // OfficialOnly is the default and must be a no-op.
        QTest::newRow("official only") << QStringLiteral("https://piston-data.mojang.com/v1/objects/a7e5a6/client.jar")
                                       << static_cast<int>(Kind::OfficialOnly)
                                       << QStringList{ QStringLiteral("https://piston-data.mojang.com/v1/objects/a7e5a6/client.jar") };

        // Query strings survive the rewrite.
        QTest::newRow("keeps query") << QStringLiteral("https://piston-meta.mojang.com/v1/packages/e61d8c/19.json?sig=abc")
                                     << static_cast<int>(Kind::MirrorOnly)
                                     << QStringList{ base + QStringLiteral("/v1/packages/e61d8c/19.json?sig=abc") };

        // Our own services follow a separate base, because a content mirror does not carry them.
        QTest::newRow("meta untouched by the content mirror") << QStringLiteral("https://meta.prismlauncher.org/v1/net.minecraft/index.json")
                                                              << static_cast<int>(Kind::MirrorOnly)
                                                              << QStringList{
                                                                 QStringLiteral("https://meta.prismlauncher.org/v1/net.minecraft/index.json") };

        QTest::newRow("translations untouched by the content mirror")
            << QStringLiteral("https://wtflauncher.github.io/i18n/index_v2.json") << static_cast<int>(Kind::MirrorThenOfficial)
            << QStringList{ QStringLiteral("https://wtflauncher.github.io/i18n/index_v2.json") };
    }

    void test_ServicesMirror()
    {
        const auto services = QStringLiteral("https://mirror.example");
        auto meta = QUrl(QStringLiteral("https://meta.prismlauncher.org/v1/net.minecraft/1.21.4.json"));
        auto i18n = QUrl(QStringLiteral("https://wtflauncher.github.io/i18n/index_v2.json"));

        // Path is preserved verbatim, including the /i18n prefix, so any crawler of the two sites works.
        QStringList expectedMeta{ QStringLiteral("https://mirror.example/v1/net.minecraft/1.21.4.json"), meta.toString() };
        QCOMPARE(toStrings(Net::MirrorPolicy::candidatesFor(meta, Kind::MirrorThenOfficial, expected_base(), services)), expectedMeta);

        QStringList expectedI18n{ QStringLiteral("https://mirror.example/i18n/index_v2.json") };
        QCOMPARE(toStrings(Net::MirrorPolicy::candidatesFor(i18n, Kind::MirrorOnly, expected_base(), services)), expectedI18n);

        // A Mojang host must keep using the content mirror even when the services base is set.
        auto jar = QUrl(QStringLiteral("https://piston-data.mojang.com/v1/objects/a7e5a6/client.jar"));
        QStringList expectedJar{ expected_base() + QStringLiteral("/v1/objects/a7e5a6/client.jar") };
        QCOMPARE(toStrings(Net::MirrorPolicy::candidatesFor(jar, Kind::MirrorOnly, expected_base(), services)), expectedJar);
    }

    void test_Rules()
    {
        QFETCH(QString, source);
        QFETCH(int, kind);
        QFETCH(QStringList, expected);

        QCOMPARE(toStrings(Net::MirrorPolicy::candidatesFor(QUrl(source), static_cast<Kind>(kind), expected_base())), expected);
    }

    void test_BadMirrorBase_data()
    {
        QTest::addColumn<QString>("base");

        QTest::newRow("empty") << QString();
        QTest::newRow("not a url") << QStringLiteral("not a url");
        QTest::newRow("no host") << QStringLiteral("file:///tmp/mirror");
    }

    void test_BadMirrorBase()
    {
        QFETCH(QString, base);
        auto url = QUrl(QStringLiteral("https://libraries.minecraft.net/com/mojang/patchy/2.2.10/patchy-2.2.10.jar"));

        // A broken setting must degrade to the official URL, never to nothing.
        auto candidates = Net::MirrorPolicy::candidatesFor(url, Kind::MirrorOnly, base);
        QCOMPARE(candidates.size(), 1);
        QCOMPARE(candidates.front(), url);
    }

    void test_TrailingSlashInBase()
    {
        auto url = QUrl(QStringLiteral("https://resources.download.minecraft.net/af/af96f5"));
        auto withSlash = toStrings(Net::MirrorPolicy::candidatesFor(url, Kind::MirrorOnly, QStringLiteral("https://mirror.example/")));
        auto withoutSlash = toStrings(Net::MirrorPolicy::candidatesFor(url, Kind::MirrorOnly, QStringLiteral("https://mirror.example")));

        QCOMPARE(withSlash, withoutSlash);
    }

    void test_BaseIsNotCachedAcrossCalls()
    {
        auto url = QUrl(QStringLiteral("https://resources.download.minecraft.net/af/af96f5"));

        // Changing the setting must take effect on the very next request.
        auto first = toStrings(Net::MirrorPolicy::candidatesFor(url, Kind::MirrorOnly, QStringLiteral("https://a.example")));
        auto second = toStrings(Net::MirrorPolicy::candidatesFor(url, Kind::MirrorOnly, QStringLiteral("https://b.example")));

        QCOMPARE(first, QStringList{ QStringLiteral("https://a.example/assets/af/af96f5") });
        QCOMPARE(second, QStringList{ QStringLiteral("https://b.example/assets/af/af96f5") });
    }

    void test_SourceSequence()
    {
        Net::SourceSequence empty;
        QVERIFY(empty.isEmpty());
        QVERIFY(!empty.advance());

        const QUrl a(QStringLiteral("https://a.example/x"));
        const QUrl b(QStringLiteral("https://b.example/x"));
        Net::SourceSequence single({ a });
        QVERIFY(!single.isEmpty());
        QCOMPARE(single.current(), a);
        QCOMPARE(single.remaining(), size_t(0));
        QVERIFY(!single.advance());  // one source means there is nowhere to fall back to
        QCOMPARE(single.current(), a);

        Net::SourceSequence two({ a, b });
        QCOMPARE(two.current(), a);
        QCOMPARE(two.remaining(), size_t(1));
        QVERIFY(two.advance());
        QCOMPARE(two.current(), b);
        QCOMPARE(two.remaining(), size_t(0));
        QVERIFY(!two.advance());
        QCOMPARE(two.current(), b);  // exhausted, and it must not run off the end
    }

    static auto expected_base() -> QString { return QStringLiteral("https://bmclapi2.bangbang93.com"); }
};

QTEST_GUILESS_MAIN(MirrorPolicyTest)

#include "MirrorPolicy_test.moc"
