#include "core/ConfParser.h"
#include "core/NmSettingsMapper.h"
#include "core/TunnelDetails.h"

#include <QtTest>

class TstTunnelDetails : public QObject {
    Q_OBJECT
private slots:
    void roundTripsMapperOutput()
    {
        const QString key = "YAnz5TF+lXXJte14tji3zlMNq+hd2rYUIgJBgB3fBmk=";
        const QString conf = QStringLiteral(
            "[Interface]\nPrivateKey = %1\nAddress = 10.0.0.2/24, fd00::2/64\nDNS = 1.1.1.1\nMTU = 1380\n"
            "[Peer]\nPublicKey = %1\nEndpoint = vpn.example.com:51820\nAllowedIPs = 0.0.0.0/0\nPersistentKeepalive = 25\n").arg(key);
        const ParseResult r = ConfParser::parse(conf);
        QVERIFY(r.ok());

        const TunnelDetails d = TunnelDetailsReader::fromSettings(NmSettingsMapper::toNmSettings(r.config, "t", "wgt"));
        QCOMPARE(d.addresses, (QStringList{"10.0.0.2/24", "fd00::2/64"}));
        QCOMPARE(d.dns, QStringList{"1.1.1.1"});
        QCOMPARE(d.mtu, 1380);
        QCOMPARE(d.peers.size(), 1);
        QCOMPARE(d.peers[0].endpoint, "vpn.example.com:51820");
        QCOMPARE(d.peers[0].allowedIps, QStringList{"0.0.0.0/0"});
        QCOMPARE(d.peers[0].persistentKeepalive, 25);
    }

    void emptySettingsGiveEmptyDetails()
    {
        const TunnelDetails d = TunnelDetailsReader::fromSettings({});
        QVERIFY(d.addresses.isEmpty());
        QVERIFY(d.peers.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstTunnelDetails)
#include "tst_tunneldetails.moc"
