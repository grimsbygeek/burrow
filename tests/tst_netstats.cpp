#include "core/NetStats.h"

#include <QtTest>

class TstNetStats : public QObject {
    Q_OBJECT

    static void writeCounters(const QString &root, const QString &iface, const QByteArray &rx, const QByteArray &tx)
    {
        QVERIFY(QDir().mkpath(root + '/' + iface + "/statistics"));
        QFile r(root + '/' + iface + "/statistics/rx_bytes");
        QVERIFY(r.open(QIODevice::WriteOnly));
        r.write(rx);
        QFile t(root + '/' + iface + "/statistics/tx_bytes");
        QVERIFY(t.open(QIODevice::WriteOnly));
        t.write(tx);
    }

private slots:
    void readsCounters()
    {
        QTemporaryDir dir;
        writeCounters(dir.path(), "wg0", "1048576\n", "2048\n");

        const NetStats::Counters c = NetStats::read("wg0", dir.path());
        QVERIFY(c.ok);
        QCOMPARE(c.rxBytes, quint64(1048576));
        QCOMPARE(c.txBytes, quint64(2048));
    }

    void missingInterfaceIsNotOk()
    {
        QTemporaryDir dir;
        QVERIFY(!NetStats::read("nope", dir.path()).ok);
    }

    void garbageIsNotOk()
    {
        QTemporaryDir dir;
        writeCounters(dir.path(), "wg0", "abc", "1");
        QVERIFY(!NetStats::read("wg0", dir.path()).ok);
    }

    void rejectsPathTricks()
    {
        QTemporaryDir dir;
        writeCounters(dir.path(), "wg0", "1", "1");
        QVERIFY(!NetStats::read("../wg0", dir.path()).ok);
        QVERIFY(!NetStats::read("", dir.path()).ok);
        QVERIFY(!NetStats::read("..", dir.path()).ok);
    }

    void formatsBytes()
    {
        QCOMPARE(NetStats::formatBytes(0), QLocale().formattedDataSize(0, 1, QLocale::DataSizeTraditionalFormat));
        QVERIFY(NetStats::formatBytes(1500000).contains("MB"));
        QVERIFY(NetStats::formatBytes(2048).contains("kB") || NetStats::formatBytes(2048).contains("KB"));
    }
};

QTEST_GUILESS_MAIN(TstNetStats)
#include "tst_netstats.moc"
