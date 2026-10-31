#include "core/ConfParser.h"
#include "core/Diagnostics.h"

#include <QtTest>

using namespace Diagnostics;
using Level = Finding::Level;

namespace {
const QString kA = QStringLiteral("YAnz5TF+lXXJte14tji3zlMNq+hd2rYUIgJBgB3fBmk=");
const QString kB = QStringLiteral("xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=");

WgConfig goodConfig()
{
    const QString text = QStringLiteral(
        "[Interface]\nPrivateKey = %1\nAddress = 10.2.0.2/32\nDNS = 10.2.0.1\n"
        "[Peer]\nPublicKey = %2\nEndpoint = 185.76.11.17:51820\nAllowedIPs = 0.0.0.0/0, ::/0\nPersistentKeepalive = 25\n").arg(kA, kB);
    const ParseResult r = ConfParser::parse(text);
    Q_ASSERT(r.ok());
    return r.config;
}

// A healthy system with a disconnected, well-formed connection.
Input healthy()
{
    Input in;
    in.nmRunning = true;
    in.nmVersion = "1.56.1";
    in.nmState = 70;
    in.connectivity = 4;
    in.kernelVersion = "6.11.2-300.fc44.x86_64";
    in.mayModifyConnections = "yes";
    in.mayControlNetwork = "yes";
    in.haveConnection = true;
    in.config = goodConfig();
    return in;
}

const Finding *find(const QList<Finding> &all, const QString &titlePart)
{
    for (const Finding &f : all)
        if (f.title.contains(titlePart, Qt::CaseInsensitive))
            return &f;
    return nullptr;
}

bool has(const QList<Finding> &all, const QString &titlePart, Level level)
{
    const Finding *f = find(all, titlePart);
    return f && f->level == level;
}
}

class TstDiagnostics : public QObject {
    Q_OBJECT
private slots:
    void healthySystemHasNoProblems()
    {
        const QList<Finding> r = run(healthy());
        QVERIFY(worstLevel(r) <= Level::Info);
        QVERIFY(has(r, "NetworkManager is running", Level::Ok));
        QVERIFY(has(r, "supports WireGuard", Level::Ok));
        QVERIFY(has(r, "settings look complete", Level::Ok));
    }

    void withoutNetworkManagerNothingElseIsChecked()
    {
        Input in = healthy();
        in.nmRunning = false;
        const QList<Finding> r = run(in);
        QCOMPARE(r.size(), 1);
        QCOMPARE(r[0].level, Level::Problem);
        QVERIFY(r[0].fix.contains("systemctl"));
    }

    void oldNetworkManagerIsAProblem()
    {
        Input in = healthy();
        in.nmVersion = "1.14.6";
        QVERIFY(has(run(in), "too old for WireGuard", Level::Problem));
        in.nmVersion = "1.16.0";
        QVERIFY(!find(run(in), "too old for WireGuard"));
        in.nmVersion = "garbage"; // unknown version: no false alarm
        QVERIFY(!find(run(in), "too old for WireGuard"));
    }

    void versionComparison()
    {
        QVERIFY(versionAtLeast("1.56.1", 1, 16));
        QVERIFY(versionAtLeast("1.16", 1, 16));
        QVERIFY(versionAtLeast("2.0.0", 1, 99));
        QVERIFY(!versionAtLeast("1.15.9", 1, 16));
        QVERIFY(!versionAtLeast("0.9.10", 1, 16));
        QVERIFY(versionAtLeast("", 1, 16));
    }

    void permissions()
    {
        Input in = healthy();
        in.mayModifyConnections = "no";
        QVERIFY(has(run(in), "isn't allowed to add or change", Level::Problem));
        in.mayModifyConnections = "auth";
        QVERIFY(has(run(in), "asked for your password", Level::Info));
        in.mayModifyConnections = "yes";
        in.mayControlNetwork = "no";
        QVERIFY(has(run(in), "turn connections on or off", Level::Problem));
        in.mayControlNetwork = "";
        in.mayModifyConnections = "";
        QVERIFY(worstLevel(run(in)) <= Level::Info); // unknown permissions: no false alarm
    }

    void connectivityStates()
    {
        Input in = healthy();
        in.connectivity = 3;
        QVERIFY(has(run(in), "no internet", Level::Warning));
        in.connectivity = 2;
        QVERIFY(has(run(in), "sign in", Level::Warning));
        QVERIFY(find(run(in), "sign in")->fix.contains("browser"));
        in.connectivity = 1;
        QVERIFY(has(run(in), "no internet connection", Level::Problem));
        in.connectivity = 0;
        QVERIFY(has(run(in), "turned off", Level::Info));
    }

    void notOnAnyNetwork()
    {
        Input in = healthy();
        in.nmState = 20;
        in.connectivity = 1;
        const QList<Finding> r = run(in);
        QVERIFY(has(r, "isn't connected to a network", Level::Problem));
        QVERIFY(!find(r, "no internet connection")); // one clear message, not two
    }

    void kernelSupport()
    {
        QVERIFY(kernelHasWireGuard("6.11.2-300.fc44.x86_64"));
        QVERIFY(kernelHasWireGuard("5.6.0"));
        QVERIFY(kernelHasWireGuard("5.15.0-91-generic"));
        QVERIFY(!kernelHasWireGuard("5.4.0-150-generic"));
        QVERIFY(!kernelHasWireGuard("4.19.0"));
        QVERIFY(!kernelHasWireGuard("nonsense"));

        Input in = healthy();
        in.kernelVersion = "5.4.0-150-generic";
        QVERIFY(has(run(in), "kernel is too old", Level::Problem));
        in.wireguardModuleLoaded = true; // module present (backported): fine
        QVERIFY(!find(run(in), "kernel is too old"));
    }

    void configProblems()
    {
        Input in = healthy();
        in.config.addresses.clear();
        QVERIFY(has(run(in), "no VPN address", Level::Problem));

        in = healthy();
        in.config.peers.clear();
        QVERIFY(has(run(in), "no server", Level::Problem));

        in = healthy();
        in.config.peers[0].publicKey = "short";
        QVERIFY(has(run(in), "public key isn't valid", Level::Problem));

        in = healthy();
        in.config.peers[0].endpoint.clear();
        QVERIFY(has(run(in), "no server address", Level::Warning));

        in = healthy();
        in.config.peers[0].allowedIps.clear();
        QVERIFY(has(run(in), "No traffic is set", Level::Problem));

        in = healthy();
        in.config.mtu = 1000;
        QVERIFY(has(run(in), "MTU", Level::Problem));
        in.config.mtu = 9000;
        QVERIFY(has(run(in), "MTU", Level::Warning));
        in.config.mtu = 1420;
        QVERIFY(!find(run(in), "MTU"));
    }

    void dnsAndIpv6Hints()
    {
        Input in = healthy();
        in.config.dnsServers.clear();
        QVERIFY(has(run(in), "No DNS server", Level::Warning));

        in = healthy();
        in.config.peers[0].allowedIps = {"0.0.0.0/0"};
        QVERIFY(has(run(in), "IPv6 traffic doesn't use the VPN", Level::Info));

        in = healthy();
        in.config.dnsServers.clear();
        in.config.peers[0].allowedIps = {"10.0.0.0/24"}; // split tunnel: no DNS warning
        QVERIFY(!find(run(in), "No DNS server"));
    }

    void slowConnect()
    {
        Input in = healthy();
        in.state = Tunnel::State::Connecting;
        in.secondsInState = 4;
        QVERIFY(has(run(in), "Connecting", Level::Info));
        in.secondsInState = 20;
        const QList<Finding> r = run(in);
        QVERIFY(has(r, "long time to connect", Level::Warning));
        QVERIFY(find(r, "long time")->fix.contains("hotspot"));
    }

    void failedLastAttempt()
    {
        Input in = healthy();
        in.lastFailure = "It timed out.";
        const QList<Finding> r = run(in);
        QVERIFY(has(r, "last attempt", Level::Problem));
        QCOMPARE(find(r, "last attempt")->detail, QString("It timed out."));
    }

    void connectedTraffic()
    {
        Input in = healthy();
        in.state = Tunnel::State::Connected;
        in.countersOk = true;

        in.sentBytes = 5000;
        in.receivedBytes = 9000;
        in.secondsInState = 60;
        QVERIFY(has(run(in), "flowing both ways", Level::Ok));

        // the classic failure: we send, nobody answers
        in.receivedBytes = 0;
        QVERIFY(has(run(in), "server isn't answering", Level::Problem));
        const QString fix = find(run(in), "server isn't answering")->fix;
        QVERIFY(fix.contains("public key"));
        QVERIFY(fix.contains("firewall"));

        // too early to judge
        in.secondsInState = 3;
        QVERIFY(has(run(in), "Waiting for the server", Level::Info));
        QVERIFY(!find(run(in), "server isn't answering"));

        // idle
        in.sentBytes = 0;
        in.secondsInState = 60;
        QVERIFY(has(run(in), "No data has used the VPN", Level::Info));

        in.countersOk = false;
        QVERIFY(has(run(in), "traffic counters", Level::Info));
    }

    void vpnDnsNotInUse()
    {
        Input in = healthy();
        in.state = Tunnel::State::Connected;
        in.countersOk = true;
        in.sentBytes = in.receivedBytes = 10;
        in.secondsInState = 60;

        in.activeDns = {"192.168.1.1"};
        QVERIFY(has(run(in), "DNS server isn't being used", Level::Warning));
        in.activeDns = {"192.168.1.1", "10.2.0.1"}; // VPN's DNS is among those in use
        QVERIFY(!find(run(in), "DNS server isn't being used"));
        in.activeDns.clear(); // unknown: stay quiet
        QVERIFY(!find(run(in), "DNS server isn't being used"));
    }

    void failureReasons()
    {
        QVERIFY(failureReasonText(6).contains("timed out"));
        QVERIFY(failureReasonText(9).contains("private key"));
        QVERIFY(failureReasonText(13).contains("kernel"));
        for (uint notFailures : {0u, 1u, 2u, 11u, 999u})
            QVERIFY2(failureReasonText(notFailures).isEmpty(), qPrintable(QString::number(notFailures)));
    }

    void redaction()
    {
        QCOMPARE(redactEndpoint("185.76.11.17:51820"), QString("185.76.11.x:51820"));
        QCOMPARE(redactEndpoint("[2a07:b944::2:2]:51820"), QString("[2a07:b944:0:0:x:x:x:x]:51820"));
        QVERIFY(!redactEndpoint("[2001:db8:1:2:3:4:5:6]:1").contains("5:6"));
        const QString named = redactEndpoint("vpn.secret-provider.com:51820");
        QVERIFY(!named.contains("secret"));
        QVERIFY(named.endsWith(":51820"));
        QCOMPARE(redactEndpoint(""), QString());
    }

    void reportContainsNoSecrets()
    {
        Input in = healthy();
        in.state = Tunnel::State::Connected;
        in.countersOk = true;
        in.sentBytes = 100;
        in.receivedBytes = 0;
        in.secondsInState = 60;
        in.appVersion = "0.1.0";
        in.osName = "Fedora Linux 44";
        in.inFlatpak = true;
        in.config.peers[0].presharedKey = kA;

        const QString text = report(in, run(in));
        QVERIFY(!text.contains(kA));                  // private + preshared key
        QVERIFY(!text.contains(kB));                  // server public key
        QVERIFY(!text.contains("185.76.11.17"));      // full server address
        QVERIFY(text.contains("185.76.11.x:51820"));
        QVERIFY(text.contains("Flatpak"));
        QVERIFY(text.contains("Fedora Linux 44"));
        QVERIFY(text.contains("[PROBLEM]"));
        QVERIFY(text.contains("What to try"));
    }
};

QTEST_APPLESS_MAIN(TstDiagnostics)
#include "tst_diagnostics.moc"
