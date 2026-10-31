#include "core/ConfParser.h"
#include "core/NmSettingsMapper.h"

#include <QtTest>

namespace {
const QString kKeyA = QStringLiteral("YAnz5TF+lXXJte14tji3zlMNq+hd2rYUIgJBgB3fBmk=");
const QString kKeyB = QStringLiteral("xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=");

QString sample()
{
    return QStringLiteral(R"(# comment
[Interface]
PrivateKey = %1
Address = 10.0.0.2/24, fd00::2/64
DNS = 1.1.1.1, 2606:4700:4700::1111, example.lan
ListenPort = 51820
MTU = 1380
PostUp = iptables -A FORWARD -i %i -j ACCEPT

[Peer]
PublicKey = %2
PresharedKey = %1
AllowedIPs = 0.0.0.0/0, ::/0
Endpoint = vpn.example.com:51820
PersistentKeepalive = 25
)").arg(kKeyA, kKeyB);
}
}

class TstConfParser : public QObject {
    Q_OBJECT
private slots:
    void parsesFullConfig()
    {
        const ParseResult r = ConfParser::parse(sample());
        QVERIFY2(r.ok(), qPrintable(r.errors.join('\n')));
        QCOMPARE(r.config.privateKey, kKeyA);
        QCOMPARE(r.config.addresses.size(), 2);
        QCOMPARE(r.config.addresses[0].ip, "10.0.0.2");
        QCOMPARE(r.config.addresses[0].prefix, 24);
        QVERIFY(r.config.addresses[1].isV6);
        QCOMPARE(r.config.dnsServers, (QStringList{"1.1.1.1", "2606:4700:4700::1111"}));
        QCOMPARE(r.config.dnsSearch, QStringList{"example.lan"});
        QCOMPARE(r.config.listenPort, 51820);
        QCOMPARE(r.config.mtu, 1380);
        QCOMPARE(r.config.peers.size(), 1);
        const WgPeer &p = r.config.peers[0];
        QCOMPARE(p.publicKey, kKeyB);
        QCOMPARE(p.allowedIps, (QStringList{"0.0.0.0/0", "::/0"}));
        QCOMPARE(p.endpoint, "vpn.example.com:51820");
        QCOMPARE(p.persistentKeepalive, 25);
        QCOMPARE(r.warnings.size(), 1); // PostUp
    }

    void rejectsBadKey()
    {
        QString text = sample();
        text.replace(kKeyA, "notakey");
        const ParseResult r = ConfParser::parse(text);
        QVERIFY(!r.ok());
    }

    void rejectsBadValues_data()
    {
        QTest::addColumn<QString>("line");
        QTest::newRow("address") << "Address = 10.0.0.300/24";
        QTest::newRow("prefix") << "Address = 10.0.0.1/33";
        QTest::newRow("port") << "ListenPort = 70000";
        QTest::newRow("endpoint") << "Endpoint = nohost";
        QTest::newRow("allowed") << "AllowedIPs = banana";
    }
    void rejectsBadValues()
    {
        QFETCH(QString, line);
        const bool peerLine = line.startsWith("Endpoint") || line.startsWith("AllowedIPs");
        const QString text = QStringLiteral("[Interface]\nPrivateKey = %1\n%2\n[Peer]\nPublicKey = %3\n%4\n")
                                 .arg(kKeyA, peerLine ? QString() : line, kKeyB, peerLine ? line : QString());
        QVERIFY(!ConfParser::parse(text).ok());
    }

    void requiresInterfaceAndPrivateKey()
    {
        QVERIFY(!ConfParser::parse("").ok());
        QVERIFY(!ConfParser::parse("[Interface]\nAddress = 10.0.0.2/32\n").ok());
    }

    void requiresPeerPublicKey()
    {
        const QString text = QStringLiteral("[Interface]\nPrivateKey = %1\n[Peer]\nEndpoint = a.b:1\n").arg(kKeyA);
        QVERIFY(!ConfParser::parse(text).ok());
    }

    void toleratesCrlfAndCase()
    {
        const QString text = QStringLiteral("[interface]\r\nprivatekey = %1\r\n[PEER]\r\npublickey=%2\r\n").arg(kKeyA, kKeyB);
        QVERIFY(ConfParser::parse(text).ok());
    }

    void sanitizesInterfaceName()
    {
        QCOMPARE(NmSettingsMapper::sanitizeInterfaceName("Home VPN"), "Home_VPN");
        QCOMPARE(NmSettingsMapper::sanitizeInterfaceName("a-very-long-tunnel-name"), "a-very-long-tun");
        QCOMPARE(NmSettingsMapper::sanitizeInterfaceName(""), "wg0");
    }

    void mapsToNmSettings()
    {
        const ParseResult r = ConfParser::parse(sample());
        const NmSettings s = NmSettingsMapper::toNmSettings(r.config, "Home", "wg-home");

        QCOMPARE(s["connection"]["type"].toString(), "wireguard");
        QCOMPARE(s["connection"]["interface-name"].toString(), "wg-home");
        QVERIFY(!s["connection"]["autoconnect"].toBool());

        QCOMPARE(s["wireguard"]["private-key"].toString(), kKeyA);
        QCOMPARE(s["wireguard"]["listen-port"].toUInt(), 51820u);
        const auto peers = s["wireguard"]["peers"].value<QList<QVariantMap>>();
        QCOMPARE(peers.size(), 1);
        QCOMPARE(peers[0]["endpoint"].toString(), "vpn.example.com:51820");
        QCOMPARE(peers[0]["allowed-ips"].toStringList(), (QStringList{"0.0.0.0/0", "::/0"}));

        QCOMPARE(s["ipv4"]["method"].toString(), "manual");
        QCOMPARE(s["ipv4"]["dns-data"].toStringList(), QStringList{"1.1.1.1"});
        QCOMPARE(s["ipv6"]["dns-data"].toStringList(), QStringList{"2606:4700:4700::1111"});
    }

    void disablesFamilyWithoutAddress()
    {
        WgConfig c;
        c.privateKey = kKeyA;
        c.addresses.append({"10.0.0.2", 32, false});
        const NmSettings s = NmSettingsMapper::toNmSettings(c, "x", "wgx");
        QCOMPARE(s["ipv4"]["method"].toString(), "manual");
        QCOMPARE(s["ipv6"]["method"].toString(), "ignore");
    }
};

QTEST_GUILESS_MAIN(TstConfParser)
#include "tst_confparser.moc"
