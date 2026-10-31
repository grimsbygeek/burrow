#include "core/KeyGen.h"
#include "core/ManualConfig.h"
#include "core/NmSettingsMapper.h"

#include <QtTest>

namespace {
const QString kPriv = QStringLiteral("YAnz5TF+lXXJte14tji3zlMNq+hd2rYUIgJBgB3fBmk=");
const QString kPub = QStringLiteral("Y2FD6uTgrq+/bbBAXCNOtE+PGk8Papoh7qaWBpQUXn0="); // from `wg pubkey`
const QString kServer = QStringLiteral("xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=");

ManualInput valid()
{
    ManualInput in;
    in.privateKey = kPriv;
    in.address = "10.0.0.2/24";
    in.serverPublicKey = kServer;
    in.endpoint = "vpn.example.com:51820";
    in.allowedIps = "0.0.0.0/0, ::/0";
    return in;
}
}

class TstManualConfig : public QObject {
    Q_OBJECT
private slots:
    void publicKeyMatchesWg()
    {
        QCOMPARE(KeyGen::publicKeyFor(kPriv), kPub);
        QCOMPARE(KeyGen::publicKeyFor(kServer), QString("+Jqdb9PFSoPelqcH4yXkIB9o5rH/KIKiUA7aHjoHwhg="));
        QCOMPARE(KeyGen::publicKeyFor("not a key"), QString());
    }

    void generatedKeysAreValidAndConsistent()
    {
        const KeyGen::KeyPair a = KeyGen::generate();
        const KeyGen::KeyPair b = KeyGen::generate();
        QVERIFY(ConfParser::isValidKey(a.privateKey));
        QVERIFY(ConfParser::isValidKey(a.publicKey));
        QCOMPARE(KeyGen::publicKeyFor(a.privateKey), a.publicKey);
        QVERIFY(a.privateKey != b.privateKey);

        const QByteArray raw = QByteArray::fromBase64(a.privateKey.toLatin1());
        QCOMPARE(int(uchar(raw[0]) & 7), 0);        // clamped like `wg genkey`
        QCOMPARE(int(uchar(raw[31]) & 0xC0), 0x40);

        QVERIFY(ConfParser::isValidKey(KeyGen::generatePresharedKey()));
    }

    void buildsValidConfig()
    {
        ManualInput in = valid();
        in.dns = "1.1.1.1";
        in.keepalive = "25";
        in.mtu = "1380";
        in.presharedKey = kPriv;
        const ParseResult r = ManualConfig::build(in);
        QVERIFY2(r.ok(), qPrintable(r.errors.join('\n')));
        QCOMPARE(r.config.addresses.size(), 1);
        QCOMPARE(r.config.dnsServers, QStringList{"1.1.1.1"});
        QCOMPARE(r.config.mtu, 1380);
        QCOMPARE(r.config.peers.size(), 1);
        QCOMPARE(r.config.peers[0].endpoint, "vpn.example.com:51820");
        QCOMPARE(r.config.peers[0].persistentKeepalive, 25);
        QCOMPARE(r.config.peers[0].allowedIps, (QStringList{"0.0.0.0/0", "::/0"}));
    }

    void reportsEveryMissingField()
    {
        const ParseResult r = ManualConfig::build({});
        QVERIFY(!r.ok());
        QCOMPARE(r.errors.size(), 5);
    }

    void errorsHaveNoLineNumbers_data()
    {
        QTest::addColumn<int>("field");
        QTest::newRow("address") << 0;
        QTest::newRow("endpoint") << 1;
        QTest::newRow("allowed") << 2;
        QTest::newRow("key") << 3;
    }
    void errorsHaveNoLineNumbers()
    {
        QFETCH(int, field);
        ManualInput in = valid();
        switch (field) {
        case 0: in.address = "10.0.0.999/24"; break;
        case 1: in.endpoint = "nohost"; break;
        case 2: in.allowedIps = "banana"; break;
        case 3: in.serverPublicKey = "short"; break;
        }
        const ParseResult r = ManualConfig::build(in);
        QVERIFY(!r.ok());
        for (const QString &e : r.errors)
            QVERIFY2(!e.startsWith("Line"), qPrintable(e));
    }

    void cannotSmuggleExtraSettings()
    {
        ManualInput in = valid();
        in.dns = "1.1.1.1\nPostUp = rm -rf /";
        QVERIFY(!ManualConfig::build(in).ok());

        in = valid();
        in.endpoint = "a.b:1\n[Peer]\nPublicKey = " + kServer;
        QVERIFY(!ManualConfig::build(in).ok());
    }

    void interfaceNamesStayUnique()
    {
        QCOMPARE(NmSettingsMapper::uniqueInterfaceName("Home", {}), "Home");
        QCOMPARE(NmSettingsMapper::uniqueInterfaceName("Home", {"Home"}), "Home-2");
        QCOMPARE(NmSettingsMapper::uniqueInterfaceName("Home", {"Home", "Home-2"}), "Home-3");
        // truncation to 15 chars must not make two long names collide
        const QString longName = "a-very-long-tunnel-name";
        const QString first = NmSettingsMapper::uniqueInterfaceName(longName, {});
        const QString second = NmSettingsMapper::uniqueInterfaceName(longName, {first});
        QVERIFY(first != second);
        QVERIFY(second.size() <= 15);
    }
};

QTEST_GUILESS_MAIN(TstManualConfig)
#include "tst_manualconfig.moc"
