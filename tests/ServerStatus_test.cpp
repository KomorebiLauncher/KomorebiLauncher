// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

#include "ui/pages/instance/ServerStatus.h"

namespace {

auto textComponent(const QString& text) -> QJsonObject
{
    return QJsonObject{ { QStringLiteral("text"), text } };
}

auto componentWithExtra(const QString& text, const QJsonArray& extra) -> QJsonObject
{
    return QJsonObject{ { QStringLiteral("text"), text }, { QStringLiteral("extra"), extra } };
}

}  // namespace

class ServerStatusTest : public QObject {
    Q_OBJECT

   private slots:
    void test_StripsFormatting()
    {
        QCOMPARE(stripFormatCodes(QStringLiteral("§a§lHello §r§7world")), QStringLiteral("Hello world"));

        // A code at the very end has no parameter to swallow.
        QCOMPARE(stripFormatCodes(QStringLiteral("trailing §")), QStringLiteral("trailing §"));
    }

    void test_FlattensMotdForms()
    {
        QCOMPARE(flattenMotd(QStringLiteral("§6A public server")), QStringLiteral("A public server"));

        auto nested = componentWithExtra(QStringLiteral("Main "), { textComponent(QStringLiteral("and more")),
                                                                    componentWithExtra(QStringLiteral(" and even more"),
                                                                                       { textComponent(QStringLiteral("!")) }) });
        QCOMPARE(flattenMotd(nested), QStringLiteral("Main and more and even more!"));

        auto array = QJsonArray{ textComponent(QStringLiteral("one ")), textComponent(QStringLiteral("two")) };
        QCOMPARE(flattenMotd(array), QStringLiteral("one two"));

        // A translated component carries no text of its own, only its arguments.
        auto translated = QJsonObject{ { QStringLiteral("translate"), QStringLiteral("some.key") },
                                       { QStringLiteral("with"), QJsonArray{ textComponent(QStringLiteral("Alice")) } } };
        QCOMPARE(flattenMotd(translated), QStringLiteral("Alice"));

        QVERIFY(flattenMotd(QJsonValue()).isEmpty());
    }

    /** A hostile server can send a component tree of any depth; flattening must not run away. */
    void test_StopsAtADepthLimit()
    {
        QJsonValue value = textComponent(QStringLiteral("leaf"));
        for (int i = 0; i < 200; i++) {
            value = componentWithExtra(QString(), { QJsonValue(value) });
        }
        auto flattened = flattenMotd(value);
        QVERIFY(flattened.size() < 10);
    }

    void test_ReadsAFullAnswer()
    {
        auto data = QJsonObject{
            { QStringLiteral("version"), QJsonObject{ { QStringLiteral("name"), QStringLiteral("1.20.4") } } },
            { QStringLiteral("players"), QJsonObject{ { QStringLiteral("max"), 20 }, { QStringLiteral("online"), 7 } } },
            { QStringLiteral("description"), textComponent(QStringLiteral("§bCome and §rcode")) },
        };

        auto status = readServerStatus(data);
        QCOMPARE(status.online, 7);
        QCOMPARE(status.max, 20);
        QCOMPARE(status.versionName, QStringLiteral("1.20.4"));
        QCOMPARE(status.motd, QStringLiteral("Come and code"));
        QVERIFY(status.isOnline());

        // The ping itself measures the latency; reading the document does not.
        QCOMPARE(status.latencyMs, -1);
    }

    /** Servers answer with less than the whole document all the time. */
    void test_ReadsPartialAnswers()
    {
        auto noPlayers = readServerStatus(QJsonObject{ { QStringLiteral("version"), QJsonObject{ { QStringLiteral("name"), QStringLiteral("1.7.10") } } } });
        QVERIFY(!noPlayers.isOnline());
        QCOMPARE(noPlayers.versionName, QStringLiteral("1.7.10"));
        QVERIFY(noPlayers.motd.isEmpty());

        auto noMax = readServerStatus(QJsonObject{ { QStringLiteral("players"), QJsonObject{ { QStringLiteral("online"), 0 } } } });
        QCOMPARE(noMax.online, 0);
        QCOMPARE(noMax.max, -1);

        // Online but with nobody on it is still online.
        QVERIFY(noMax.isOnline());

        auto empty = readServerStatus(QJsonObject());
        QVERIFY(!empty.isOnline());
        QVERIFY(empty.versionName.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ServerStatusTest)

#include "ServerStatus_test.moc"
