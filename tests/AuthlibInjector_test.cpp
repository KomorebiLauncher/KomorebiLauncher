// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

#include <RuntimeContext.h>

#include "minecraft/Library.h"
#include "minecraft/OneSixVersionFormat.h"
#include "minecraft/VersionFile.h"
#include "modplatform/authlibinjector/AuthlibInjector.h"

class AuthlibInjectorTest : public QObject {
    Q_OBJECT

    // Shape copied from the live .../mirrors/authlib-injector/artifact/latest.json. Written as
    // adjacent literals because moc does not survive a raw string literal in this file.
    static auto fixture() -> QByteArray
    {
        return QByteArray("{\"build_number\": 56,"
                          " \"version\": \"1.2.8\","
                          " \"download_url\": \"https://bmclapi2.bangbang93.com/mirrors/authlib-injector/artifact/56/authlib-injector-1.2.8.jar\","
                          " \"checksums\": { \"sha256\": \"9c7f4343e6c82034958ffb48c14a2cb0c85928be7283103ce17da00c6d5a7b10\" } }");
    }

   private slots:
    void test_ParseRelease()
    {
        auto release = AuthlibInjector::parseRelease(fixture());
        QCOMPARE(release.version, QStringLiteral("1.2.8"));
        QCOMPARE(release.sha256, QStringLiteral("9c7f4343e6c82034958ffb48c14a2cb0c85928be7283103ce17da00c6d5a7b10"));
        QVERIFY(release.downloadUrl.endsWith(QStringLiteral("/authlib-injector-1.2.8.jar")));
    }

    void test_ParseReleaseRefusesPartialAnswers()
    {
        // Without a checksum there is nothing to pin the jar to, so the answer is unusable.
        QVERIFY(AuthlibInjector::parseRelease("{\"version\":\"1.2.8\",\"download_url\":\"https://x/y.jar\"}").version.isEmpty());
        QVERIFY(AuthlibInjector::parseRelease("not json").version.isEmpty());
        QVERIFY(AuthlibInjector::parseRelease("[\"version\"]").version.isEmpty());
    }

    /** The jar has to be saved where the launch will look for it, which is the maven layout under the storage prefix. */
    void test_AgentLibraryResolvesToTheMavenLayout()
    {
        auto release = AuthlibInjector::parseRelease(fixture());
        Library library;
        library.setRawName(GradleSpecifier(AuthlibInjector::mavenName + release.version));

        RuntimeContext context;
        context.system = QStringLiteral("windows");
        context.javaArchitecture = QStringLiteral("64");
        context.javaRealArchitecture = QStringLiteral("amd64");

        QStringList jars;
        QStringList native;
        QStringList native32;
        QStringList native64;
        library.getApplicableFiles(context, jars, native, native32, native64, QString());

        QCOMPARE(jars.size(), 1);
        auto path = QDir::fromNativeSeparators(jars.at(0));
        QVERIFY2(path.endsWith(QStringLiteral("/libraries/yushijinhun/authlib-injector/1.2.8/authlib-injector-1.2.8.jar")), qPrintable(path));
    }

    void test_BuildComponent()
    {
        auto release = AuthlibInjector::parseRelease(fixture());
        auto component = AuthlibInjector::buildComponent(release, QStringLiteral("https://auth.example.com"), QStringLiteral("1.20.4"),
                                                         QStringLiteral("file:///C:/inst/libraries/authlib-injector-1.2.8.jar"),
                                                         QStringLiteral("0123456789abcdef0123456789abcdef01234567"), 349681);

        QCOMPARE(component.value("uid").toString(), QStringLiteral("wtf.authlib-injector"));
        QCOMPARE(component.value("version").toString(), QStringLiteral("1.2.8"));

        auto requirement = component.value("requires").toArray().at(0).toObject();
        QCOMPARE(requirement.value("uid").toString(), QStringLiteral("net.minecraft"));
        QCOMPARE(requirement.value("equals").toString(), QStringLiteral("1.20.4"));

        auto agent = component.value("+agents").toArray().at(0).toObject();
        QCOMPARE(agent.value("name").toString(), QStringLiteral("yushijinhun:authlib-injector:1.2.8"));
        // This is how the agent is told where to send the client's authentication requests.
        QCOMPARE(agent.value("argument").toString(), QStringLiteral("https://auth.example.com"));

        auto artifact = agent.value("downloads").toObject().value("artifact").toObject();
        QCOMPARE(artifact.value("url").toString(), QStringLiteral("file:///C:/inst/libraries/authlib-injector-1.2.8.jar"));
        QCOMPARE(artifact.value("sha1").toString(), QStringLiteral("0123456789abcdef0123456789abcdef01234567"));
        QCOMPARE(artifact.value("size").toVariant().toLongLong(), 349681LL);

        // Listing it under libraries as well would also put the agent on the classpath.
        QVERIFY(!component.contains("libraries"));
    }

    /** What has to hold is that the component system reads this back as an agent with an argument. */
    void test_ComponentParsesBackAsAnAgent()
    {
        auto release = AuthlibInjector::parseRelease(fixture());
        auto component = AuthlibInjector::buildComponent(release, QStringLiteral("https://auth.example.com"), QStringLiteral("1.20.4"),
                                                         QStringLiteral("file:///C:/inst/libraries/authlib-injector-1.2.8.jar"),
                                                         QStringLiteral("aa"), 349681);

        auto parsed = OneSixVersionFormat::versionFileFromJson(QJsonDocument(component), QStringLiteral("wtf.authlib-injector.json"), false);
        QVERIFY2(parsed.has_value(), parsed.has_value() ? "" : qPrintable(parsed.error()));

        auto file = parsed.value();
        QCOMPARE(file->uid, QStringLiteral("wtf.authlib-injector"));
        QCOMPARE(file->agents.size(), 1);
        QCOMPARE(file->agents.at(0).argument, QStringLiteral("https://auth.example.com"));
        QVERIFY(file->agents.at(0).library != nullptr);
        QVERIFY(file->libraries.isEmpty());
    }
};

QTEST_GUILESS_MAIN(AuthlibInjectorTest)

#include "AuthlibInjector_test.moc"
