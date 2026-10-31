#include "core/NmSettingsMapper.h"
#include "nm/NmClient.h"

#include <QtTest>
#include <algorithm>

namespace {
NmSettings settingsFor(const QString &id)
{
    WgConfig c;
    c.privateKey = "YAnz5TF+lXXJte14tji3zlMNq+hd2rYUIgJBgB3fBmk=";
    c.addresses.append({"10.0.0.2", 32, false});
    WgPeer peer;
    peer.publicKey = "xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=";
    peer.endpoint = "vpn.example.com:51820";
    peer.allowedIps = {"0.0.0.0/0"};
    c.peers.append(peer);
    return NmSettingsMapper::toNmSettings(c, id, "wgtest");
}
}

// Runs against tests/fake_nm.py on a private session bus.
class TstNmIntegration : public QObject {
    Q_OBJECT
private slots:
    void connectAndDisconnect()
    {
        NmClient client(QDBusConnection::sessionBus());
        QVERIFY(client.isAvailable());

        client.refresh();
        QCOMPARE(client.tunnels().size(), 0);

        QVERIFY(!client.addConnection(settingsFor("home")).path().isEmpty());
        QTRY_COMPARE(client.tunnels().size(), 1);
        const Tunnel before = client.tunnels().first();
        QCOMPARE(before.id, "home");
        QCOMPARE(before.state, Tunnel::State::Disconnected);

        client.connectTunnel(before.uuid);
        QTRY_COMPARE(client.tunnels().first().state, Tunnel::State::Connecting);
        QTRY_COMPARE(client.tunnels().first().state, Tunnel::State::Connected);
        QVERIFY(!client.tunnels().first().activePath.path().isEmpty());

        client.disconnectTunnel(before.uuid);
        QTRY_COMPARE(client.tunnels().first().state, Tunnel::State::Disconnected);
        QVERIFY(client.tunnels().first().activePath.path().isEmpty());
    }

    void exportReadsSecretsBack()
    {
        NmClient client(QDBusConnection::sessionBus());
        client.refresh();

        WgConfig c;
        c.privateKey = "YAnz5TF+lXXJte14tji3zlMNq+hd2rYUIgJBgB3fBmk=";
        c.addresses.append({"10.0.0.2", 32, false});
        c.dnsServers << "10.0.0.1";
        WgPeer peer;
        peer.publicKey = "xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=";
        peer.presharedKey = "+Jqdb9PFSoPelqcH4yXkIB9o5rH/KIKiUA7aHjoHwhg=";
        peer.endpoint = "vpn.example.com:51820";
        peer.allowedIps = {"0.0.0.0/0", "::/0"};
        peer.persistentKeepalive = 25;
        c.peers.append(peer);

        QVERIFY(!client.addConnection(NmSettingsMapper::toNmSettings(c, "exportme", "wgexport")).path().isEmpty());
        Tunnel t;
        for (const Tunnel &candidate : client.tunnels())
            if (candidate.id == "exportme")
                t = candidate;
        QVERIFY(!t.uuid.isEmpty());

        // what GetSettings gives out must not contain the secrets...
        QVERIFY(!t.settings["wireguard"].contains("private-key"));
        QVERIFY(NmSettingsMapper::fromNmSettings(t.settings, {}).privateKey.isEmpty());

        // ...but GetSecrets + GetSettings together give the original config back
        NmSettings secrets;
        QVERIFY(client.getSecrets(t.uuid, secrets));
        const WgConfig back = NmSettingsMapper::fromNmSettings(t.settings, secrets);
        QCOMPARE(back.privateKey, c.privateKey);
        QCOMPARE(back.addresses.size(), 1);
        QCOMPARE(back.dnsServers, c.dnsServers);
        QCOMPARE(back.peers.size(), 1);
        QCOMPARE(back.peers[0].presharedKey, peer.presharedKey);
        QCOMPARE(back.peers[0].endpoint, peer.endpoint);
        QCOMPARE(back.peers[0].allowedIps, peer.allowedIps);
        QCOMPARE(back.peers[0].persistentKeepalive, 25);

        NmSettings none;
        QVERIFY(!client.getSecrets("no-such-uuid", none));
    }

    void systemInfoReadsNetworkManager()
    {
        NmClient client(QDBusConnection::sessionBus());
        const NmSystemInfo info = client.systemInfo();
        QVERIFY(info.running);
        QCOMPARE(info.version, "1.56.1");
        QCOMPARE(info.state, 70);
        QCOMPARE(info.connectivity, 4);
        QCOMPARE(info.mayModifyConnections, "yes");
        QCOMPARE(info.mayControlNetwork, "auth");
        QCOMPARE(info.dnsServers, (QStringList{"10.2.0.1", "192.168.1.1"}));
    }

    void failedActivationExplainsWhy()
    {
        NmClient client(QDBusConnection::sessionBus());
        client.refresh();
        QVERIFY(!client.addConnection(settingsFor("timeout")).path().isEmpty());

        const auto find = [&client](const QString &id) {
            for (const Tunnel &t : client.tunnels())
                if (t.id == id)
                    return t;
            return Tunnel{};
        };
        const QString uuid = find("timeout").uuid;
        QVERIFY(!uuid.isEmpty());
        QVERIFY(find("timeout").failure.isEmpty());

        client.connectTunnel(uuid);
        QTRY_VERIFY(!find("timeout").failure.isEmpty());
        QVERIFY2(find("timeout").failure.contains("timed out"), qPrintable(find("timeout").failure));
        QCOMPARE(find("timeout").state, Tunnel::State::Disconnected);

        // a fresh attempt clears the old failure while it runs
        client.connectTunnel(uuid);
        QTRY_VERIFY(find("timeout").failure.isEmpty() || find("timeout").state == Tunnel::State::Connecting);
        QTRY_VERIFY(!find("timeout").failure.isEmpty()); // and it fails again, with the reason
    }

    void aUserDisconnectIsNotAFailure()
    {
        NmClient client(QDBusConnection::sessionBus());
        client.refresh();
        QVERIFY(!client.addConnection(settingsFor("polite")).path().isEmpty());
        QString uuid;
        for (const Tunnel &t : client.tunnels())
            if (t.id == "polite")
                uuid = t.uuid;

        client.connectTunnel(uuid);
        QTRY_VERIFY(std::any_of(client.tunnels().cbegin(), client.tunnels().cend(),
                                [](const Tunnel &t) { return t.id == "polite" && t.state == Tunnel::State::Connected; }));
        client.disconnectTunnel(uuid);
        QTRY_VERIFY(std::any_of(client.tunnels().cbegin(), client.tunnels().cend(),
                                [](const Tunnel &t) { return t.id == "polite" && t.state == Tunnel::State::Disconnected; }));
        for (const Tunnel &t : client.tunnels())
            if (t.id == "polite")
                QVERIFY(t.failure.isEmpty());
    }

    void deleteRemovesTunnel()
    {
        NmClient client(QDBusConnection::sessionBus());
        client.refresh();
        QVERIFY(!client.addConnection(settingsFor("doomed")).path().isEmpty());

        const auto find = [&client](const QString &id) {
            for (const Tunnel &t : client.tunnels())
                if (t.id == id)
                    return t;
            return Tunnel{};
        };
        const Tunnel doomed = find("doomed");
        QVERIFY(!doomed.uuid.isEmpty());

        client.deleteTunnel("no-such-uuid"); // unknown uuid is ignored
        QCOMPARE(find("doomed").uuid, doomed.uuid);

        client.deleteTunnel(doomed.uuid);
        QTRY_VERIFY(find("doomed").uuid.isEmpty());
    }

    void reportsActivationFailure()
    {
        NmClient client(QDBusConnection::sessionBus());
        client.refresh();
        QVERIFY(!client.addConnection(settingsFor("fail")).path().isEmpty());

        QString uuid;
        for (const Tunnel &t : client.tunnels())
            if (t.id == "fail")
                uuid = t.uuid;
        QVERIFY(!uuid.isEmpty());

        QSignalSpy errors(&client, &NmClient::errorOccurred);
        client.connectTunnel(uuid);
        QTRY_COMPARE(errors.size(), 1);
        QVERIFY(errors.first().first().toString().contains("refused"));
    }

    void mapsNmStates()
    {
        QCOMPARE(NmClient::stateFromNm(0), Tunnel::State::Disconnected);
        QCOMPARE(NmClient::stateFromNm(1), Tunnel::State::Connecting);
        QCOMPARE(NmClient::stateFromNm(2), Tunnel::State::Connected);
        QCOMPARE(NmClient::stateFromNm(3), Tunnel::State::Disconnecting);
        QCOMPARE(NmClient::stateFromNm(4), Tunnel::State::Disconnected);
    }
};

QTEST_GUILESS_MAIN(TstNmIntegration)
#include "tst_nmintegration.moc"
