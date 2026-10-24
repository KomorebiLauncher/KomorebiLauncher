// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QtTest>

#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"

class AccountUsageTest : public QObject {
    Q_OBJECT

   private slots:
    /** A local profile proves no ownership, so it is not enough to launch or to download. */
    void test_OfflineProfileIsNotUsable()
    {
        AccountList accounts;
        accounts.addAccount(MinecraftAccount::createOffline(QStringLiteral("Steve")));

        QVERIFY(!accounts.anyAccountUsable());
        QVERIFY(!accounts.anyAccountIsValid());
    }

    /** A third-party server is a real sign-in path, so it counts without a Microsoft account. */
    void test_ThirdPartyAccountIsUsable()
    {
        AccountList accounts;
        accounts.addAccount(MinecraftAccount::createYggdrasil(QStringLiteral("https://example.invalid/api/yggdrasil"),
                                                              QStringLiteral("player"), QStringLiteral("secret")));

        QVERIFY(accounts.anyAccountUsable());
        QVERIFY(!accounts.anyAccountIsValid());
    }

    /** A Microsoft account which has not read an entitlement yet does not count either. */
    void test_MicrosoftAccountWithoutOwnershipIsNotUsable()
    {
        AccountList accounts;
        accounts.addAccount(MinecraftAccount::createBlankMSA());

        QVERIFY(!accounts.anyAccountUsable());
    }
};

QTEST_GUILESS_MAIN(AccountUsageTest)

#include "AccountUsage_test.moc"
