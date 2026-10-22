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
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/Parsers.h"

namespace {

auto jsonOf(const QJsonObject& object) -> QByteArray
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

auto character(const QString& id, const QString& name) -> QJsonObject
{
    return QJsonObject{ { QStringLiteral("id"), id }, { QStringLiteral("name"), name } };
}

}  // namespace

class YggdrasilAuthTest : public QObject {
    Q_OBJECT

   private slots:
    /** The answer to a sign-in carries the token the game launches with and the character it plays. */
    void test_ParseAuthenticate()
    {
        auto answer = QJsonObject{ { QStringLiteral("accessToken"), QStringLiteral("a-token") },
                                   { QStringLiteral("clientToken"), QStringLiteral("a-client") },
                                   { QStringLiteral("selectedProfile"), character(QStringLiteral("deadbeef"), QStringLiteral("Kujou")) } };

        Token token;
        MinecraftProfile profile;
        QVERIFY(Parsers::parseYggdrasilResponse(jsonOf(answer), token, profile));

        QCOMPARE(token.token, QStringLiteral("a-token"));
        QVERIFY(token.validity == Validity::Certain);
        QCOMPARE(token.extra.value(QStringLiteral("clientToken")).toString(), QStringLiteral("a-client"));
        QVERIFY(!token.notAfter.isValid());

        QCOMPARE(profile.id, QStringLiteral("deadbeef"));
        QCOMPARE(profile.name, QStringLiteral("Kujou"));
        QVERIFY(profile.validity == Validity::Certain);
    }

    /** A refresh that names no character still renews the token - the account keeps who it already is. */
    void test_ParseRefreshWithoutProfile()
    {
        auto body = jsonOf(QJsonObject{ { QStringLiteral("accessToken"), QStringLiteral("new-token") },
                                        { QStringLiteral("clientToken"), QStringLiteral("a-client") } });

        Token token;
        MinecraftProfile profile;
        profile.id = QStringLiteral("deadbeef");
        profile.name = QStringLiteral("Kujou");
        QVERIFY(Parsers::parseYggdrasilResponse(body, token, profile));

        QCOMPARE(token.token, QStringLiteral("new-token"));
        QCOMPARE(profile.name, QStringLiteral("Kujou"));
    }

    /** A half answer must not leave a half account behind. */
    void test_RejectsIncompleteResponses()
    {
        Token token;
        MinecraftProfile profile;

        auto noToken = jsonOf(QJsonObject{ { QStringLiteral("clientToken"), QStringLiteral("a-client") } });
        QVERIFY(!Parsers::parseYggdrasilResponse(noToken, token, profile));

        auto brokenProfile = jsonOf(QJsonObject{ { QStringLiteral("accessToken"), QStringLiteral("a-token") },
                                                 { QStringLiteral("selectedProfile"), character(QStringLiteral("deadbeef"), QString()) } });
        QVERIFY(!Parsers::parseYggdrasilResponse(brokenProfile, token, profile));

        QVERIFY(!Parsers::parseYggdrasilResponse("not json", token, profile));
    }

    /** The reason a server refuses a login is only in the body of the error answer. */
    void test_ReadsTheRefusal()
    {
        auto body = jsonOf(QJsonObject{ { QStringLiteral("error"), QStringLiteral("ForbiddenOperationException") },
                                        { QStringLiteral("errorMessage"), QStringLiteral("Invalid credentials.") } });
        QCOMPARE(Parsers::parseYggdrasilError(body), QStringLiteral("Invalid credentials."));

        QVERIFY(Parsers::parseYggdrasilError("Forbidden").isEmpty());
    }

    /** Whatever the server says, the account has to survive a restart with its credentials. */
    void test_AccountRoundTrip()
    {
        AccountData source;
        source.type = AccountType::Yggdrasil;
        source.authServerUrl = QStringLiteral("https://auth.example.com");
        source.loginName = QStringLiteral("kujou");
        source.password = QStringLiteral("hunter2");
        source.yggdrasilToken.token = QStringLiteral("a-token");
        source.yggdrasilToken.extra[QStringLiteral("clientToken")] = QStringLiteral("a-client");
        source.minecraftProfile.id = QStringLiteral("deadbeef");
        source.minecraftProfile.name = QStringLiteral("Kujou");
        source.minecraftProfile.validity = Validity::Certain;

        AccountData restored;
        QVERIFY(restored.resumeStateFromV3(source.saveState()));

        QVERIFY(restored.type == AccountType::Yggdrasil);
        QCOMPARE(restored.authServerUrl, QStringLiteral("https://auth.example.com"));
        QCOMPARE(restored.loginName, QStringLiteral("kujou"));
        QCOMPARE(restored.password, QStringLiteral("hunter2"));
        QCOMPARE(restored.accessToken(), QStringLiteral("a-token"));
        QCOMPARE(restored.profileId(), QStringLiteral("deadbeef"));
        QCOMPARE(restored.profileName(), QStringLiteral("Kujou"));
        QCOMPARE(restored.yggdrasilToken.extra.value(QStringLiteral("clientToken")).toString(), QStringLiteral("a-client"));
    }
};

QTEST_GUILESS_MAIN(YggdrasilAuthTest)

#include "YggdrasilAuth_test.moc"
