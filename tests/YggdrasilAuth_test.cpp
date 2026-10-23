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
#include "minecraft/auth/steps/YggdrasilStep.h"
#include "ui/dialogs/YggdrasilLoginDialog.h"

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
        QList<YggdrasilProfile> profiles;
        QVERIFY(Parsers::parseYggdrasilResponse(jsonOf(answer), token, profile, profiles));

        QCOMPARE(token.token, QStringLiteral("a-token"));
        QVERIFY(token.validity == Validity::Certain);
        QCOMPARE(token.extra.value(QStringLiteral("clientToken")).toString(), QStringLiteral("a-client"));
        QVERIFY(!token.notAfter.isValid());

        QCOMPARE(profile.id, QStringLiteral("deadbeef"));
        QCOMPARE(profile.name, QStringLiteral("Kujou"));
        QVERIFY(profile.validity == Validity::Certain);
    }

    /** A server that lets one login hold several characters says so in the same answer, and the launcher has to hear it. */
    void test_ReadsTheCharactersOnOffer()
    {
        auto answer = QJsonObject{ { QStringLiteral("accessToken"), QStringLiteral("a-token") },
                                   { QStringLiteral("selectedProfile"), character(QStringLiteral("beefcafe"), QStringLiteral("Shiori")) },
                                   { QStringLiteral("availableProfiles"),
                                     QJsonArray{ character(QStringLiteral("beefcafe"), QStringLiteral("Shiori")),
                                                 character(QStringLiteral("deadbeef"), QStringLiteral("Kujou")),
                                                 QJsonObject{ { QStringLiteral("name"), QStringLiteral("Ghost") } } } } };

        Token token;
        MinecraftProfile profile;
        QList<YggdrasilProfile> profiles;
        QVERIFY(Parsers::parseYggdrasilResponse(jsonOf(answer), token, profile, profiles));

        // A character with no id cannot be asked for, so it is not offered even though the rest of the answer is fine.
        QCOMPARE(profiles.size(), 2);
        QCOMPARE(profiles.at(0).id, QStringLiteral("beefcafe"));
        QCOMPARE(profiles.at(1).name, QStringLiteral("Kujou"));
    }

    /** An answer that offers nothing leaves nothing to choose from. */
    void test_NoCharactersOffered()
    {
        auto answer = QJsonObject{ { QStringLiteral("accessToken"), QStringLiteral("a-token") },
                                   { QStringLiteral("selectedProfile"), character(QStringLiteral("deadbeef"), QStringLiteral("Kujou")) },
                                   { QStringLiteral("availableProfiles"), QJsonArray() } };

        Token token;
        MinecraftProfile profile;
        QList<YggdrasilProfile> profiles{ YggdrasilProfile{ QStringLiteral("old"), QStringLiteral("Stale") } };
        QVERIFY(Parsers::parseYggdrasilResponse(jsonOf(answer), token, profile, profiles));
        QCOMPARE(profiles.size(), 0);
    }

    /** A refresh that names no character still renews the token - the account keeps who it already is. */
    void test_ParseRefreshWithoutProfile()
    {
        auto body = jsonOf(QJsonObject{ { QStringLiteral("accessToken"), QStringLiteral("new-token") },
                                        { QStringLiteral("clientToken"), QStringLiteral("a-client") } });

        Token token;
        MinecraftProfile profile;
        QList<YggdrasilProfile> profiles;
        profile.id = QStringLiteral("deadbeef");
        profile.name = QStringLiteral("Kujou");
        QVERIFY(Parsers::parseYggdrasilResponse(body, token, profile, profiles));

        QCOMPARE(token.token, QStringLiteral("new-token"));
        QCOMPARE(profile.name, QStringLiteral("Kujou"));
    }

    /** A half answer must not leave a half account behind. */
    void test_RejectsIncompleteResponses()
    {
        Token token;
        MinecraftProfile profile;
        QList<YggdrasilProfile> profiles;

        auto noToken = jsonOf(QJsonObject{ { QStringLiteral("clientToken"), QStringLiteral("a-client") } });
        QVERIFY(!Parsers::parseYggdrasilResponse(noToken, token, profile, profiles));

        auto brokenProfile = jsonOf(QJsonObject{ { QStringLiteral("accessToken"), QStringLiteral("a-token") },
                                                 { QStringLiteral("selectedProfile"), character(QStringLiteral("deadbeef"), QString()) } });
        QVERIFY(!Parsers::parseYggdrasilResponse(brokenProfile, token, profile, profiles));

        QVERIFY(!Parsers::parseYggdrasilResponse("not json", token, profile, profiles));
    }

    /** The reason a server refuses a login is only in the body of the error answer. */
    void test_ReadsTheRefusal()
    {
        auto body = jsonOf(QJsonObject{ { QStringLiteral("error"), QStringLiteral("ForbiddenOperationException") },
                                        { QStringLiteral("errorMessage"), QStringLiteral("Invalid credentials.") } });
        QCOMPARE(Parsers::parseYggdrasilError(body), QStringLiteral("Invalid credentials."));

        QVERIFY(Parsers::parseYggdrasilError("Forbidden").isEmpty());
    }

    /** The server address is the root of the APIs it hosts, so the operation goes below authserver. */
    void test_PutsTheOperationBelowAuthserver()
    {
        QCOMPARE(YggdrasilStep::endpoint(QStringLiteral("https://littleskin.cn/api/yggdrasil"), QStringLiteral("authenticate")).toString(),
                 QStringLiteral("https://littleskin.cn/api/yggdrasil/authserver/authenticate"));
        QCOMPARE(YggdrasilStep::endpoint(QStringLiteral(" https://auth.example.com/yggdrasil/ "), QStringLiteral("refresh")).toString(),
                 QStringLiteral("https://auth.example.com/yggdrasil/authserver/refresh"));
    }

    /** The spec asks for the agent version as a number, and a string is refused before the login is looked at. */
    void test_SendsTheShapeTheSpecRequires()
    {
        AccountData data;
        data.loginName = QStringLiteral("kujou");
        data.password = QStringLiteral("hunter2");
        data.yggdrasilToken.extra[QStringLiteral("clientToken")] = QStringLiteral("cccc");

        auto body = YggdrasilStep::requestBody(data, true);
        auto agent = body.value(QStringLiteral("agent")).toObject();
        QCOMPARE(agent.value(QStringLiteral("name")).toString(), QStringLiteral("Minecraft"));
        auto version = agent.value(QStringLiteral("version"));
        QVERIFY2(version.isDouble(), "the agent version has to be a number, not a string");
        QCOMPARE(version.toInt(), 1);
        QCOMPARE(body.value(QStringLiteral("username")).toString(), QStringLiteral("kujou"));
        QCOMPARE(body.value(QStringLiteral("password")).toString(), QStringLiteral("hunter2"));
        QCOMPARE(body.value(QStringLiteral("requestUser")).toBool(), true);
        QCOMPARE(body.value(QStringLiteral("clientToken")).toString(), QStringLiteral("cccc"));
        QVERIFY(!agent.contains(QStringLiteral("id")));

        auto refresh = YggdrasilStep::requestBody(data, false);
        QVERIFY(!refresh.contains(QStringLiteral("agent")));
        QVERIFY(!refresh.contains(QStringLiteral("password")));

        data.profileChoice = QStringLiteral("deadbeef");
        auto chosen = YggdrasilStep::requestBody(data, true);
        QCOMPARE(chosen.value(QStringLiteral("agent")).toObject().value(QStringLiteral("id")).toString(), QStringLiteral("deadbeef"));
    }

    /** Whatever the server says, the account has to survive a restart with its credentials. */
    void test_AccountRoundTrip()
    {
        AccountData source;
        source.type = AccountType::Yggdrasil;
        source.authServerUrl = QStringLiteral("https://auth.example.com");
        source.loginName = QStringLiteral("kujou");
        source.password = QStringLiteral("hunter2");
        source.profileChoice = QStringLiteral("deadbeef");
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
        QCOMPARE(restored.profileChoice, QStringLiteral("deadbeef"));
        QCOMPARE(restored.accessToken(), QStringLiteral("a-token"));
        QCOMPARE(restored.profileId(), QStringLiteral("deadbeef"));
        QCOMPARE(restored.profileName(), QStringLiteral("Kujou"));
        QCOMPARE(restored.yggdrasilToken.extra.value(QStringLiteral("clientToken")).toString(), QStringLiteral("a-client"));
    }
    /** A pasted sentence must not become part of the address, because no server can read it back. */
    void test_RejectsAnAddressWithOtherTextInIt()
    {
        QVERIFY(YggdrasilLoginDialog::serverProblem(QStringLiteral("https://littleskin.cn/api/yggdrasil")).isEmpty());
        QVERIFY(YggdrasilLoginDialog::serverProblem(QStringLiteral(" http://127.0.0.1:8000/yggdrasil ")).isEmpty());
        QVERIFY(YggdrasilLoginDialog::serverProblem(QStringLiteral("   ")).isEmpty());

        QVERIFY(!YggdrasilLoginDialog::serverProblem(QStringLiteral("http://127.0.0.1:8000/yggdrasil，用户名随便")).isEmpty());
        QVERIFY(!YggdrasilLoginDialog::serverProblem(QStringLiteral("http://a.example/x y")).isEmpty());
        QVERIFY(!YggdrasilLoginDialog::serverProblem(QStringLiteral("littleskin.cn/api/yggdrasil")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(YggdrasilAuthTest)

#include "YggdrasilAuth_test.moc"
