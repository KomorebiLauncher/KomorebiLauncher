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

#include "minecraft/auth/MSAErrors.h"

/**
 * Which sign-in failures the launcher can blame on its own application id.
 *
 * The advice shown to the player differs between "your account was refused" and "the app id this
 * build uses was refused": only the second one is something they can work around, so the match has
 * to be neither too wide nor too narrow.
 */
class MSAErrorsTest : public QObject {
    Q_OBJECT

   private slots:
    void test_RecognizesAppIdFailures_data()
    {
        QTest::addColumn<QString>("reason");
        QTest::addColumn<bool>("isAppIdProblem");

        // What Microsoft sends when the client id is missing from the directory, is registered as a
        // confidential client, or is paired with an unregistered redirect URI.
        QTest::newRow("app not found")
            << "AADSTS700016: Application with identifier 'x' was not found in the directory 'Microsoft Accounts'."
            << true;
        QTest::newRow("needs client secret") << "AADSTS7000218: The request body must contain the following parameter: 'client_secret'." << true;
        QTest::newRow("bad client secret") << "AADSTS7000215: Invalid client secret is provided." << true;
        QTest::newRow("redirect uri") << "AADSTS9002325: Proof Key for Code Exchange is required." << true;
        QTest::newRow("oauth invalid_client") << "invalid_client: Client authentication failed." << true;
        QTest::newRow("oauth unauthorized_client") << "unauthorized_client" << true;
        QTest::newRow("mojang wording") << "Invalid app registration" << true;
        QTest::newRow("mixed case") << "aadsts700016" << true;

        // Failures that belong to the player or the network: telling them to change the app id
        // would send them off to fix the wrong thing.
        QTest::newRow("wrong password") << "AADSTS50126: Error validating credentials due to invalid username or password." << false;
        QTest::newRow("consent declined") << "AADSTS65004: User declined to consent to access the app." << false;
        QTest::newRow("network") << "Host not found" << false;
        QTest::newRow("empty") << "" << false;
    }

    void test_RecognizesAppIdFailures()
    {
        QFETCH(QString, reason);
        QFETCH(bool, isAppIdProblem);

        QCOMPARE(MSA::describesAppIdProblem(reason), isAppIdProblem);
    }
};

QTEST_GUILESS_MAIN(MSAErrorsTest)

#include "MSAErrors_test.moc"
