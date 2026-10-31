// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QtTest>

#include "minecraft/auth/AccountData.h"
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

    /** The offline gate asks for a Microsoft account whose license check passed, not just any account. */
    void test_OfflineGateNeedsAnOwningMicrosoftAccount()
    {
        AccountList accounts;
        accounts.addAccount(MinecraftAccount::createOffline(QStringLiteral("Steve")));
        accounts.addAccount(MinecraftAccount::createYggdrasil(QStringLiteral("https://example.invalid/api/yggdrasil"),
                                                              QStringLiteral("player"), QStringLiteral("secret")));
        QVERIFY(!accounts.anyMicrosoftAccountOwnsMinecraft());

        AccountData owning;
        owning.type = AccountType::MSA;
        owning.minecraftEntitlement.ownsMinecraft = true;
        owning.minecraftEntitlement.canPlayMinecraft = true;
        owning.minecraftEntitlement.validity = Validity::Certain;
        auto account = MinecraftAccount::loadFromJsonV3(owning.saveState());
        QVERIFY(account);

        accounts.addAccount(account);
        QVERIFY(accounts.anyMicrosoftAccountOwnsMinecraft());
    }

    /** A third-party server marking its account as an owner must not unlock local profiles. */
    void test_ThirdPartyOwnershipDoesNotOpenTheOfflineGate()
    {
        AccountData thirdParty;
        thirdParty.type = AccountType::Yggdrasil;
        thirdParty.authServerUrl = QStringLiteral("https://example.invalid/api/yggdrasil");
        thirdParty.minecraftEntitlement.ownsMinecraft = true;
        thirdParty.minecraftEntitlement.canPlayMinecraft = true;
        thirdParty.minecraftEntitlement.validity = Validity::Certain;
        auto account = MinecraftAccount::loadFromJsonV3(thirdParty.saveState());
        QVERIFY(account);

        AccountList accounts;
        accounts.addAccount(account);
        QVERIFY(accounts.anyAccountUsable());
        QVERIFY(!accounts.anyMicrosoftAccountOwnsMinecraft());
    }
};

QTEST_GUILESS_MAIN(AccountUsageTest)

#include "AccountUsage_test.moc"
