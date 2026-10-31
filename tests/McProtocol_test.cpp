// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Komorebi Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include <QTest>

#include "ui/pages/instance/McProtocol.h"

/**
 * The bytes the status handshake puts on the wire.
 *
 * These are the values from wiki.vg, written out literally: the handshake has no length delimiter
 * the server can resynchronise on, so one wrong bit is an answer that never comes, and the only
 * way to notice is to compare against the documented encoding.
 */
class McProtocolTest : public QObject {
    Q_OBJECT

   private slots:
    void test_VarIntMatchesTheDocumentedBytes_data()
    {
        QTest::addColumn<int>("value");
        QTest::addColumn<QByteArray>("encoded");

        QTest::newRow("zero") << 0 << QByteArray("\x00", 1);
        QTest::newRow("one") << 1 << QByteArray("\x01", 1);
        QTest::newRow("largest in one byte") << 127 << QByteArray("\x7f", 1);
        QTest::newRow("first needing two bytes") << 128 << QByteArray("\x80\x01", 2);
        QTest::newRow("two bytes") << 255 << QByteArray("\xff\x01", 2);
        QTest::newRow("protocol 1.20.1") << 763 << QByteArray("\xfb\x05", 2);
        QTest::newRow("three bytes") << 16384 << QByteArray("\x80\x80\x01", 3);
        // The protocol writes signed values as five-byte two's complement; a negative value must
        // still terminate rather than shifting its sign in forever.
        QTest::newRow("minus one") << -1 << QByteArray("\xff\xff\xff\xff\x0f", 5);
    }

    void test_VarIntMatchesTheDocumentedBytes()
    {
        QFETCH(int, value);
        QFETCH(QByteArray, encoded);

        QByteArray data;
        McProtocol::writeVarInt(data, value);
        QCOMPARE(data, encoded);
    }

    void test_StringPrefixCountsBytesNotCharacters_data()
    {
        QTest::addColumn<QString>("value");
        QTest::addColumn<QByteArray>("prefix");
        QTest::addColumn<int>("byteLength");

        QTest::newRow("empty") << QString() << QByteArray("\x00", 1) << 0;
        QTest::newRow("ascii 127") << QString(127, QLatin1Char('a')) << QByteArray("\x7f", 1) << 127;
        // 128 is the first length that needs a second VarInt byte; writing 0 here is what used to
        // truncate long addresses and oversized answers.
        QTest::newRow("ascii 128") << QString(128, QLatin1Char('a')) << QByteArray("\x80\x01", 2) << 128;
        // U+00E9 is one UTF-16 code unit but two UTF-8 bytes.
        QTest::newRow("two-byte utf8") << QStringLiteral("\u00e9") << QByteArray("\x02", 1) << 2;
        // Three characters, nine bytes: a prefix of 3 would cut the string in the middle.
        QTest::newRow("three-byte utf8") << QStringLiteral("\u670d\u52a1\u5668") << QByteArray("\x09", 1) << 9;
    }

    void test_StringPrefixCountsBytesNotCharacters()
    {
        QFETCH(QString, value);
        QFETCH(QByteArray, prefix);
        QFETCH(int, byteLength);

        QByteArray data;
        McProtocol::writeString(data, value);

        const auto bytes = value.toUtf8();
        QCOMPARE(bytes.size(), byteLength);
        QCOMPARE(data.left(prefix.size()), prefix);
        QCOMPARE(data.mid(prefix.size()), bytes);
    }
};

QTEST_GUILESS_MAIN(McProtocolTest)

#include "McProtocol_test.moc"
