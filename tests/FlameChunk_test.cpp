// SPDX-License-Identifier: GPL-3.0-only
#include <QTest>

#include "modplatform/flame/FlameAPI.h"

class FlameChunkTest : public QObject {
    Q_OBJECT

   private slots:
    void splitsIntoRequestSizedBatches();
    void keepsTheOrderOfTheIds();
    void givesNothingForNoIds();
};

void FlameChunkTest::splitsIntoRequestSizedBatches()
{
    QStringList ids;
    for (int index = 0; index < 250; ++index)
        ids.append(QString::number(index));

    const auto batches = FlameAPI::chunkFileIds(ids, 100);
    QCOMPARE(batches.size(), 3);
    QCOMPARE(batches.at(0).size(), 100);
    QCOMPARE(batches.at(1).size(), 100);
    QCOMPARE(batches.at(2).size(), 50);
}

void FlameChunkTest::keepsTheOrderOfTheIds()
{
    const QStringList ids{ QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c") };
    const auto batches = FlameAPI::chunkFileIds(ids, 2);
    QCOMPARE(batches.size(), 2);
    QCOMPARE(batches.at(0), (QStringList{ QStringLiteral("a"), QStringLiteral("b") }));
    QCOMPARE(batches.at(1), (QStringList{ QStringLiteral("c") }));
}

void FlameChunkTest::givesNothingForNoIds()
{
    QVERIFY(FlameAPI::chunkFileIds({}, 100).isEmpty());
}

QTEST_GUILESS_MAIN(FlameChunkTest)

#include "FlameChunk_test.moc"