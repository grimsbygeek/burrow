#include "core/ConfParser.h"
#include "core/ConfWriter.h"
#include "core/NmSettingsMapper.h"
#include "core/QrCode.h"

#include <QtTest>

namespace {
const QString kA = QStringLiteral("YAnz5TF+lXXJte14tji3zlMNq+hd2rYUIgJBgB3fBmk=");
const QString kB = QStringLiteral("xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=");

const QString kSample = QStringLiteral(
    "[Interface]\nPrivateKey = %1\nAddress = 10.2.0.2/32, 2a07:b944::2:2/128\nDNS = 10.2.0.1, 2a07:b944::2:1, corp.lan\n"
    "ListenPort = 51820\nMTU = 1380\nFwMark = 0x1234\n\n"
    "[Peer]\nPublicKey = %2\nPresharedKey = %1\nAllowedIPs = 0.0.0.0/0, ::/0\nEndpoint = 185.76.11.17:51820\nPersistentKeepalive = 25\n"
    "\n[Peer]\nPublicKey = %1\nAllowedIPs = 10.9.0.0/16\n").arg(kA, kB);

void compare(const WgConfig &a, const WgConfig &b)
{
    QCOMPARE(a.privateKey, b.privateKey);
    QCOMPARE(a.addresses.size(), b.addresses.size());
    for (int i = 0; i < a.addresses.size(); ++i) {
        QCOMPARE(a.addresses[i].ip, b.addresses[i].ip);
        QCOMPARE(a.addresses[i].prefix, b.addresses[i].prefix);
        QCOMPARE(a.addresses[i].isV6, b.addresses[i].isV6);
    }
    QCOMPARE(a.dnsServers, b.dnsServers);
    QCOMPARE(a.dnsSearch, b.dnsSearch);
    QCOMPARE(a.listenPort, b.listenPort);
    QCOMPARE(a.mtu, b.mtu);
    QCOMPARE(a.fwmark, b.fwmark);
    QCOMPARE(a.peers.size(), b.peers.size());
    for (int i = 0; i < a.peers.size(); ++i) {
        QCOMPARE(a.peers[i].publicKey, b.peers[i].publicKey);
        QCOMPARE(a.peers[i].presharedKey, b.peers[i].presharedKey);
        QCOMPARE(a.peers[i].allowedIps, b.peers[i].allowedIps);
        QCOMPARE(a.peers[i].endpoint, b.peers[i].endpoint);
        QCOMPARE(a.peers[i].persistentKeepalive, b.peers[i].persistentKeepalive);
    }
}

// What NetworkManager keeps out of GetSettings and serves from GetSecrets instead.
void splitSecrets(NmSettings &settings, NmSettings &secrets)
{
    QVariantMap &wg = settings["wireguard"];
    secrets["wireguard"]["private-key"] = wg.take("private-key");

    QList<QVariantMap> peers = wg.value("peers").value<QList<QVariantMap>>();
    QList<QVariantMap> secretPeers;
    for (QVariantMap &p : peers) {
        if (p.contains("preshared-key")) {
            secretPeers.append({{"public-key", p.value("public-key")}, {"preshared-key", p.take("preshared-key")}});
            p.remove("preshared-key-flags");
        }
    }
    wg["peers"] = QVariant::fromValue(peers);
    if (!secretPeers.isEmpty())
        secrets["wireguard"]["peers"] = QVariant::fromValue(secretPeers);
}
}

class TstExport : public QObject {
    Q_OBJECT
private slots:
    void writerOutputParsesBackIdentically()
    {
        const ParseResult original = ConfParser::parse(kSample);
        QVERIFY2(original.ok(), qPrintable(original.errors.join('\n')));

        const QString text = ConfWriter::toWgQuick(original.config);
        const ParseResult again = ConfParser::parse(text);
        QVERIFY2(again.ok(), qPrintable(again.errors.join('\n')));
        QVERIFY(again.warnings.isEmpty());
        compare(original.config, again.config);
    }

    void survivesNetworkManagerRoundTrip()
    {
        const WgConfig original = ConfParser::parse(kSample).config;

        NmSettings settings = NmSettingsMapper::toNmSettings(original, "x", "wgx");
        NmSettings secrets;
        splitSecrets(settings, secrets);
        QVERIFY(!settings["wireguard"].contains("private-key")); // as NM would return it

        compare(original, NmSettingsMapper::fromNmSettings(settings, secrets));
    }

    void withoutSecretsThePrivateKeyIsMissing()
    {
        NmSettings settings = NmSettingsMapper::toNmSettings(ConfParser::parse(kSample).config, "x", "wgx");
        NmSettings secrets;
        splitSecrets(settings, secrets);
        QVERIFY(NmSettingsMapper::fromNmSettings(settings, {}).privateKey.isEmpty());
    }

    void savedFileIsPrivate()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("a.conf");

        // pre-existing and world-readable: must end up owner-only
        { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("old"); f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadOther); }

        QVERIFY(ConfWriter::saveFile(path, kSample).isEmpty());
        QFile f(path);
        const QFileDevice::Permissions perms = f.permissions();
        QVERIFY(perms.testFlag(QFileDevice::ReadOwner) && perms.testFlag(QFileDevice::WriteOwner));
        const QFileDevice::Permissions others = QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
            | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther | QFileDevice::ExeOwner;
        QVERIFY2(!(perms & others), "file must be readable by its owner only");
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(f.readAll()), kSample);
    }

    void saveFailureIsReported()
    {
        QVERIFY(!ConfWriter::saveFile("/nonexistent-dir/x.conf", "x").isEmpty());
    }

    void qrIsSquareWithQuietZone()
    {
        const QImage img = QrCode::render(kSample, 320);
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), img.height());
        QVERIFY(img.width() >= 320);
        QCOMPARE(img.pixelColor(0, 0), QColor(Qt::white));
        QCOMPARE(img.pixelColor(img.width() - 1, img.height() - 1), QColor(Qt::white));

        bool anyBlack = false;
        for (int y = 0; y < img.height() && !anyBlack; ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixelColor(x, y) == QColor(Qt::black)) { anyBlack = true; break; }
        QVERIFY(anyBlack);
    }

    void qrRefusesWhatCannotFit()
    {
        QVERIFY(QrCode::render(QString(5000, 'x')).isNull());
        QVERIFY(QrCode::render(QString()).isNull());
    }
};

QTEST_MAIN(TstExport)
#include "tst_export.moc"
