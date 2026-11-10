// SPDX-License-Identifier: GPL-3.0-only
#include <QDateTime>
#include <QTemporaryDir>
#include <QTest>

#include "komorebi/services/auth/AccountStore.h"
#include "komorebi/services/auth/TokenVault.h"

using namespace Komorebi::Services;

namespace {

AuthSession microsoftSession()
{
    AuthSession session;
    session.username = QStringLiteral("Notch");
    session.uuid = QStringLiteral("069a79f444e94726a5befca90e38aaf5");
    session.accessToken = QStringLiteral("an-access-token");
    session.refreshToken = QStringLiteral("a-refresh-token");
    session.xuid = QStringLiteral("123456789");
    session.accessExpires = QDateTime::currentDateTimeUtc().addSecs(3600);
    return session;
}

}  // namespace

class AccountStoreTest : public QObject {
    Q_OBJECT

   private slots:
    void offlineNameGetsTheGamesUuid();
    void offlineNameIsAWholeSession();
    void emptyNameIsNoSession();
    void anEmptyVaultIsNotABrokenOne();
    void offlineNeedsAMicrosoftAccount();
    void offlineStandsBesideAMicrosoftAccount();
    void offlineSessionSurvivesARoundTrip();
    void theLastMicrosoftAccountCannotBeRemovedWhileAnOfflineOneRemains();
};

void AccountStoreTest::offlineNameGetsTheGamesUuid()
{
    // The vanilla launcher's uuid for a name nobody ever verified is the starting point for worlds
    // shared with it: the same name has to be the same player.
    const AuthSession session = offlineSession(QStringLiteral("Notch"));
    QCOMPARE(session.uuid, QStringLiteral("b50ad385829d3141a2167e7d7539ba7f"));

    // Version 3 and the IETF variant, stamped the way Java's UUID#nameUUIDFromBytes does it.
    QCOMPARE(session.uuid.at(12), QChar(u'3'));
    QVERIFY(QStringLiteral("89ab").contains(session.uuid.at(16)));

    QCOMPARE(offlineSession(QStringLiteral("notch")).uuid, QStringLiteral("42653081a90e3475b3d63550cdb43f8e"));
}

void AccountStoreTest::offlineNameIsAWholeSession()
{
    const AuthSession session = offlineSession(QStringLiteral("  Steve  "));
    QVERIFY(session.offline);
    QCOMPARE(session.username, QStringLiteral("Steve"));
    QVERIFY(!session.uuid.isEmpty());
    QCOMPARE(session.accessToken, QStringLiteral("0"));
    QVERIFY(session.refreshToken.isEmpty());
    QVERIFY(session.isValid());
    // Nothing expires: there is no sign-in to go stale.
    QVERIFY(!session.expired());
}

void AccountStoreTest::emptyNameIsNoSession()
{
    QVERIFY(!offlineSession(QString()).isValid());
    QVERIFY(!offlineSession(QStringLiteral("   ")).isValid());
}

void AccountStoreTest::anEmptyVaultIsNotABrokenOne()
{
    // A data root that never signed in has no file at all; that is a state, not an error to report.
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString error;
    const AccountStore store = AccountStore::forDataRoot(directory.path());
    QVERIFY(store.accounts(&error).isEmpty());
    QVERIFY2(error.isEmpty(), qPrintable(error));
}

void AccountStoreTest::offlineNeedsAMicrosoftAccount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const AccountStore store = AccountStore::forDataRoot(directory.path());
    QString why;
    QVERIFY(!store.canAddOffline(&why));
    QVERIFY(!why.isEmpty());

    QString error;
    QVERIFY(!store.addOffline(QStringLiteral("Steve"), nullptr, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(store.accounts().isEmpty());

    // The same rule applies from the other direction: an offline name is not a Microsoft sign-in.
    QVERIFY(!store.addMicrosoft(offlineSession(QStringLiteral("Steve")), &error));
}

void AccountStoreTest::offlineStandsBesideAMicrosoftAccount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const AccountStore store = AccountStore::forDataRoot(directory.path());
    QVERIFY(store.addMicrosoft(microsoftSession()));
    QVERIFY(store.canAddOffline());

    AuthSession added;
    QString error;
    QVERIFY2(store.addOffline(QStringLiteral("Steve"), &added, &error), qPrintable(error));
    QVERIFY(added.offline);
    QCOMPARE(added.username, QStringLiteral("Steve"));
    QCOMPARE(added.accessToken, QStringLiteral("0"));

    const QList<AuthSession> accounts = store.accounts();
    QCOMPARE(accounts.size(), 2);
}

void AccountStoreTest::offlineSessionSurvivesARoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString error;
    {
        const AccountStore store = AccountStore::forDataRoot(directory.path());
        QVERIFY(store.addMicrosoft(microsoftSession(), &error));
        QVERIFY2(store.addOffline(QStringLiteral("Steve"), nullptr, &error), qPrintable(error));
    }

    // A different store over the same directory: what the next launch of the launcher sees.
    const QList<AuthSession> accounts = AccountStore::forDataRoot(directory.path()).accounts();
    QCOMPARE(accounts.size(), 2);

    bool sawOffline = false;
    for (const auto& account : accounts) {
        if (!account.offline)
            continue;
        sawOffline = true;
        QCOMPARE(account.username, QStringLiteral("Steve"));
        QCOMPARE(account.uuid, offlineSession(QStringLiteral("Steve")).uuid);
        QCOMPARE(account.accessToken, QStringLiteral("0"));
        QVERIFY(account.refreshToken.isEmpty());
        QVERIFY(account.isValid());
    }
    QVERIFY(sawOffline);
}

void AccountStoreTest::theLastMicrosoftAccountCannotBeRemovedWhileAnOfflineOneRemains()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const AccountStore store = AccountStore::forDataRoot(directory.path());
    const AuthSession microsoft = microsoftSession();
    QVERIFY(store.addMicrosoft(microsoft));
    AuthSession offline;
    QVERIFY(store.addOffline(QStringLiteral("Steve"), &offline));

    // Removing the Microsoft account first would leave the offline name with nothing to stand beside.
    QString error;
    QVERIFY(!store.remove(microsoft.uuid, &error));
    QVERIFY(error.contains(QStringLiteral("offline")));
    QCOMPARE(store.accounts().size(), 2);

    // Removing the offline one first is allowed, and then so is the Microsoft one.
    QVERIFY2(store.remove(offline.uuid, &error), qPrintable(error));
    QVERIFY2(store.remove(microsoft.uuid, &error), qPrintable(error));
    QVERIFY(store.accounts().isEmpty());
}

QTEST_GUILESS_MAIN(AccountStoreTest)

#include "KomorebiAccountStore_test.moc"
