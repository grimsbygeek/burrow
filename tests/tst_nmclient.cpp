#include "nm/NmClient.h"

#include <QtTest>

class TstNmClient : public QObject {
    Q_OBJECT
private slots:
    void parsesWireguardConnection()
    {
        NmSettings s;
        s["connection"] = {{"type", "wireguard"}, {"id", "Home"}, {"uuid", "u-1"},
                           {"interface-name", "wg0"}, {"autoconnect", false}};
        Tunnel t;
        QVERIFY(NmClient::tunnelFromSettings(QDBusObjectPath("/c/1"), s, t));
        QCOMPARE(t.id, "Home");
        QCOMPARE(t.uuid, "u-1");
        QCOMPARE(t.interfaceName, "wg0");
        QVERIFY(!t.autoconnect);
    }

    void autoconnectDefaultsToTrue()
    {
        NmSettings s;
        s["connection"] = {{"type", "wireguard"}, {"id", "x"}};
        Tunnel t;
        QVERIFY(NmClient::tunnelFromSettings(QDBusObjectPath("/c/1"), s, t));
        QVERIFY(t.autoconnect);
    }

    void ignoresOtherTypes()
    {
        NmSettings s;
        s["connection"] = {{"type", "802-11-wireless"}, {"id", "Wifi"}};
        Tunnel t;
        QVERIFY(!NmClient::tunnelFromSettings(QDBusObjectPath("/c/2"), s, t));
        QVERIFY(!NmClient::tunnelFromSettings(QDBusObjectPath("/c/3"), {}, t));
    }
};

QTEST_GUILESS_MAIN(TstNmClient)
#include "tst_nmclient.moc"
