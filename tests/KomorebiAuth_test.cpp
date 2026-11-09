// SPDX-License-Identifier: GPL-3.0-only
#include <QDir>
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include "komorebi/services/auth/MicrosoftAuth.h"
#include "komorebi/services/auth/TokenVault.h"

using namespace Komorebi::Services;

namespace {

QByteArray json(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

/// Answers each path with a body of the test's choosing, and remembers what it was asked.
class AuthServer : public QTcpServer {
   public:
    struct Reply {
        int status = 200;
        QByteArray body;
    };

    QHash<QByteArray, Reply> replies;
    QHash<QByteArray, QByteArray> lastBody;
    QStringList order;

   protected:
    void incomingConnection(qintptr handle) override
    {
        auto* socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);
        auto buffer = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer] {
            buffer->append(socket->readAll());
            const int headerEnd = buffer->indexOf("\r\n\r\n");
            if (headerEnd < 0)
                return;

            const QByteArray head = buffer->left(headerEnd);
            int contentLength = 0;
            for (const auto& line : head.split('\n')) {
                const QByteArray trimmed = line.trimmed();
                if (trimmed.toLower().startsWith("content-length:"))
                    contentLength = trimmed.mid(15).trimmed().toInt();
            }
            if (buffer->size() < headerEnd + 4 + contentLength)
                return;

            const QByteArray body = buffer->mid(headerEnd + 4, contentLength);
            buffer->clear();
            const QByteArray path = head.split('\n').first().split(' ').value(1);
            order << QString::fromLatin1(path);
            lastBody[path] = body;

            const Reply reply = replies.value(path, Reply{ 404, json(QJsonObject{}) });
            socket->write("HTTP/1.1 " + QByteArray::number(reply.status)
                          + " Answer\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: "
                          + QByteArray::number(reply.body.size()) + "\r\n\r\n");
            socket->write(reply.body);
            socket->flush();
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
};

MicrosoftAuth::Endpoints endpointsFor(quint16 port)
{
    const auto url = [port](const char* path) {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(port).arg(QString::fromLatin1(path)));
    };
    MicrosoftAuth::Endpoints endpoints;
    endpoints.deviceCode = url("/devicecode");
    endpoints.token = url("/token");
    endpoints.xboxUser = url("/xbox");
    endpoints.xsts = url("/xsts");
    endpoints.minecraftLogin = url("/login");
    endpoints.minecraftProfile = url("/profile");
    return endpoints;
}

/// The answers every successful chain needs after the Microsoft token step.
void answerTheRestOfTheChain(AuthServer& server)
{
    server.replies["/xbox"] = { 200, json({ { QStringLiteral("Token"), QStringLiteral("xbox-token") } }) };
    server.replies["/xsts"] = {
        200,
        json({ { QStringLiteral("Token"), QStringLiteral("xsts-token") },
               { QStringLiteral("DisplayClaims"),
                 QJsonObject{ { QStringLiteral("xui"),
                                QJsonArray{ QJsonObject{ { QStringLiteral("uhs"), QStringLiteral("user-hash") },
                                                         { QStringLiteral("xid"), QStringLiteral("12345") } } } } } } })
    };
    server.replies["/login"] = { 200, json({ { QStringLiteral("access_token"), QStringLiteral("minecraft-token") } }) };
    server.replies["/profile"] = { 200, json({ { QStringLiteral("name"), QStringLiteral("ManakaGekka") },
                                               { QStringLiteral("id"), QStringLiteral("98cccdf2b71c463b8f049df92d828da5") } }) };
}

}  // namespace

class KomorebiAuthTest : public QObject {
    Q_OBJECT

   private slots:
    void refreshWalksTheWholeChain();
    void deviceCodeWaitsForTheBrowser();
    void reportsAnAccountWithoutXbox();
    void reportsAnAccountThatDoesNotOwnMinecraft();
    void refusesToSignInWithoutAnAppId();
    void vaultRoundTripsTokens();
    void vaultRejectsAnUnreadableFile();
    void expiredSessionsSaySo();
};

void KomorebiAuthTest::refreshWalksTheWholeChain()
{
    AuthServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.replies["/token"] = { 200, json({ { QStringLiteral("access_token"), QStringLiteral("microsoft-token") },
                                             { QStringLiteral("refresh_token"), QStringLiteral("second-refresh") },
                                             { QStringLiteral("expires_in"), 3600 } }) };
    answerTheRestOfTheChain(server);

    QNetworkAccessManager network;
    MicrosoftAuth auth(&network);
    auth.setClientId(QStringLiteral("test-client"));
    auth.setEndpoints(endpointsFor(server.serverPort()));

    QSignalSpy signedIn(&auth, &MicrosoftAuth::signedIn);
    QSignalSpy failed(&auth, &MicrosoftAuth::failed);
    auth.refresh(QStringLiteral("first-refresh"));

    QTRY_VERIFY_WITH_TIMEOUT(signedIn.count() == 1 || failed.count() == 1, 15000);
    QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.first().at(0).toString()));

    const AuthSession session = auth.session();
    QCOMPARE(session.username, QStringLiteral("ManakaGekka"));
    QCOMPARE(session.uuid, QStringLiteral("98cccdf2b71c463b8f049df92d828da5"));
    QCOMPARE(session.accessToken, QStringLiteral("minecraft-token"));
    QCOMPARE(session.refreshToken, QStringLiteral("second-refresh"));
    QCOMPARE(session.xuid, QStringLiteral("12345"));
    QVERIFY(session.accessExpires.isValid());
    QVERIFY(!session.expired());

    // Each step got what the step before it produced, and they happened in order.
    const QStringList expectedOrder{ QStringLiteral("/token"), QStringLiteral("/xbox"), QStringLiteral("/xsts"),
                                     QStringLiteral("/login"), QStringLiteral("/profile") };
    QCOMPARE(server.order, expectedOrder);
    QVERIFY(server.lastBody.value("/token").contains("refresh_token=first-refresh"));
    QVERIFY(server.lastBody.value("/xbox").contains("d=microsoft-token"));
    QVERIFY(server.lastBody.value("/login").contains("XBL3.0 x=user-hash;xsts-token"));
}

void KomorebiAuthTest::deviceCodeWaitsForTheBrowser()
{
    AuthServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.replies["/devicecode"] = { 200, json({ { QStringLiteral("device_code"), QStringLiteral("secret-code") },
                                                  { QStringLiteral("user_code"), QStringLiteral("ABCD-1234") },
                                                  { QStringLiteral("verification_uri"), QStringLiteral("https://www.microsoft.com/link") },
                                                  { QStringLiteral("interval"), 1 },
                                                  { QStringLiteral("expires_in"), 900 } }) };
    server.replies["/token"] = { 400, json({ { QStringLiteral("error"), QStringLiteral("authorization_pending") } }) };

    QNetworkAccessManager network;
    MicrosoftAuth auth(&network);
    auth.setClientId(QStringLiteral("test-client"));
    auth.setEndpoints(endpointsFor(server.serverPort()));

    QSignalSpy codeReady(&auth, &MicrosoftAuth::deviceCodeReady);
    QSignalSpy pending(&auth, &MicrosoftAuth::pending);
    QSignalSpy slowDown(&auth, &MicrosoftAuth::slowDown);
    QSignalSpy signedIn(&auth, &MicrosoftAuth::signedIn);
    QSignalSpy failed(&auth, &MicrosoftAuth::failed);

    auth.requestDeviceCode();
    QTRY_COMPARE_WITH_TIMEOUT(codeReady.count(), 1, 10000);
    QCOMPARE(auth.currentDeviceCode().userCode, QStringLiteral("ABCD-1234"));
    QCOMPARE(auth.currentDeviceCode().deviceCode, QStringLiteral("secret-code"));
    QCOMPARE(auth.currentDeviceCode().interval, 1);

    // The browser is still open: Microsoft says "not yet", which is not an error.
    auth.pollDeviceCode(auth.currentDeviceCode());
    QTRY_COMPARE_WITH_TIMEOUT(pending.count(), 1, 10000);
    QVERIFY(failed.isEmpty());

    // Too many polls in a row: Microsoft asks for a slower pace.
    server.replies["/token"] = { 400, json({ { QStringLiteral("error"), QStringLiteral("slow_down") } }) };
    auth.pollDeviceCode(auth.currentDeviceCode());
    QTRY_COMPARE_WITH_TIMEOUT(slowDown.count(), 1, 10000);
    QVERIFY(failed.isEmpty());

    // The player approved; the rest of the chain follows the same way a refresh would.
    server.replies["/token"] = { 200, json({ { QStringLiteral("access_token"), QStringLiteral("microsoft-token") },
                                             { QStringLiteral("refresh_token"), QStringLiteral("the-refresh") },
                                             { QStringLiteral("expires_in"), 3600 } }) };
    answerTheRestOfTheChain(server);
    auth.pollDeviceCode(auth.currentDeviceCode());
    QTRY_VERIFY_WITH_TIMEOUT(signedIn.count() == 1 || failed.count() == 1, 15000);
    QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.first().at(0).toString()));
    QCOMPARE(auth.session().refreshToken, QStringLiteral("the-refresh"));
}

void KomorebiAuthTest::reportsAnAccountWithoutXbox()
{
    AuthServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.replies["/token"] = { 200, json({ { QStringLiteral("access_token"), QStringLiteral("microsoft-token") },
                                             { QStringLiteral("refresh_token"), QStringLiteral("the-refresh") } }) };
    server.replies["/xbox"] = { 200, json({ { QStringLiteral("Token"), QStringLiteral("xbox-token") } }) };
    server.replies["/xsts"] = { 401, json({ { QStringLiteral("XErr"), QStringLiteral("2148916233") } }) };

    QNetworkAccessManager network;
    MicrosoftAuth auth(&network);
    auth.setClientId(QStringLiteral("test-client"));
    auth.setEndpoints(endpointsFor(server.serverPort()));

    QSignalSpy signedIn(&auth, &MicrosoftAuth::signedIn);
    QSignalSpy failed(&auth, &MicrosoftAuth::failed);
    auth.refresh(QStringLiteral("first-refresh"));

    QTRY_VERIFY_WITH_TIMEOUT(signedIn.count() == 1 || failed.count() == 1, 15000);
    QVERIFY(signedIn.isEmpty());
    QVERIFY(failed.first().at(0).toString().contains(QStringLiteral("no Xbox profile")));
}

void KomorebiAuthTest::reportsAnAccountThatDoesNotOwnMinecraft()
{
    AuthServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    server.replies["/token"] = { 200, json({ { QStringLiteral("access_token"), QStringLiteral("microsoft-token") },
                                             { QStringLiteral("refresh_token"), QStringLiteral("the-refresh") } }) };
    answerTheRestOfTheChain(server);
    server.replies["/profile"] = { 404, json({ { QStringLiteral("error"), QStringLiteral("NOT_FOUND") } }) };

    QNetworkAccessManager network;
    MicrosoftAuth auth(&network);
    auth.setClientId(QStringLiteral("test-client"));
    auth.setEndpoints(endpointsFor(server.serverPort()));

    QSignalSpy signedIn(&auth, &MicrosoftAuth::signedIn);
    QSignalSpy failed(&auth, &MicrosoftAuth::failed);
    auth.refresh(QStringLiteral("first-refresh"));

    QTRY_VERIFY_WITH_TIMEOUT(signedIn.count() == 1 || failed.count() == 1, 15000);
    QVERIFY(signedIn.isEmpty());
    QVERIFY(failed.first().at(0).toString().contains(QStringLiteral("does not own Minecraft")));
}

void KomorebiAuthTest::refusesToSignInWithoutAnAppId()
{
    QNetworkAccessManager network;
    MicrosoftAuth auth(&network);
    QSignalSpy failed(&auth, &MicrosoftAuth::failed);
    auth.refresh(QStringLiteral("first-refresh"));
    QCOMPARE(failed.count(), 1);
    QVERIFY(failed.first().at(0).toString().contains(QStringLiteral("no Microsoft app id")));
}

void KomorebiAuthTest::vaultRoundTripsTokens()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const TokenVault vault = TokenVault::forDataRoot(directory.path());
    AuthSession account;
    account.username = QStringLiteral("ManakaGekka");
    account.uuid = QStringLiteral("98cccdf2b71c463b8f049df92d828da5");
    account.xuid = QStringLiteral("12345");
    account.refreshToken = QStringLiteral("a-refresh-token-that-must-not-be-readable");

    QString error;
    QVERIFY2(vault.save({ account }, &error), qPrintable(error));

    QFile file(vault.path());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray raw = file.readAll();
    file.close();
    QVERIFY(!raw.contains("a-refresh-token-that-must-not-be-readable"));

    const QList<AuthSession> loaded = vault.load(&error);
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().username, QStringLiteral("ManakaGekka"));
    QCOMPARE(loaded.first().uuid, account.uuid);
    QCOMPARE(loaded.first().xuid, account.xuid);
    QCOMPARE(loaded.first().refreshToken, account.refreshToken);

    // A second sign-in of the same account replaces the entry instead of adding another.
    account.refreshToken = QStringLiteral("the-newer-token");
    QVERIFY2(vault.upsert(account, &error), qPrintable(error));
    QCOMPARE(vault.load().size(), 1);
    QCOMPARE(vault.load().first().refreshToken, QStringLiteral("the-newer-token"));

    QVERIFY2(vault.remove(account.uuid, &error), qPrintable(error));
    QVERIFY(vault.load().isEmpty());
}

void KomorebiAuthTest::vaultRejectsAnUnreadableFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const TokenVault vault = TokenVault::forDataRoot(directory.path());

    QFile file(vault.path());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not json");
    file.close();

    QString error;
    QVERIFY(vault.load(&error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void KomorebiAuthTest::expiredSessionsSaySo()
{
    AuthSession session;
    QVERIFY(!session.expired());  // unknown is not the same as expired

    session.accessExpires = QDateTime::currentDateTimeUtc().addSecs(-60);
    QVERIFY(session.expired());

    session.accessExpires = QDateTime::currentDateTimeUtc().addSecs(60);
    QVERIFY(!session.expired());
}

QTEST_GUILESS_MAIN(KomorebiAuthTest)

#include "KomorebiAuth_test.moc"
