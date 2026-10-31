#include "core/Diagnostics.h"

#include "core/ConfParser.h"

#include <QHostAddress>
#include <QLocale>
#include <QObject>
#include <QRegularExpression>
#include <algorithm>

using Level = Diagnostics::Finding::Level;
using Finding = Diagnostics::Finding;

namespace {

Finding make(Level level, const QString &title, const QString &detail = {}, const QString &fix = {})
{
    return {level, title, detail, fix};
}

QString bytes(quint64 n)
{
    return QLocale().formattedDataSize(qint64(n), 1, QLocale::DataSizeTraditionalFormat);
}

QString hostOf(const QString &endpoint)
{
    if (endpoint.startsWith('[')) // [v6]:port
        return endpoint.mid(1, endpoint.indexOf(']') - 1);
    return endpoint.left(endpoint.lastIndexOf(':'));
}

QString portOf(const QString &endpoint)
{
    return endpoint.mid(endpoint.lastIndexOf(':') + 1);
}

void systemChecks(const Diagnostics::Input &in, QList<Finding> &out)
{
    out << make(Level::Ok, QObject::tr("NetworkManager is running"),
                in.nmVersion.isEmpty() ? QString() : QObject::tr("Version %1.").arg(in.nmVersion));

    // NetworkManager gained WireGuard support in 1.16
    if (!in.nmVersion.isEmpty() && !Diagnostics::versionAtLeast(in.nmVersion, 1, 16)) {
        out << make(Level::Problem, QObject::tr("NetworkManager is too old for WireGuard"),
                    QObject::tr("You have version %1. WireGuard needs 1.16 or newer.").arg(in.nmVersion),
                    QObject::tr("Update your system, or use a newer distribution release."));
    }

    if (in.mayModifyConnections == QLatin1String("no")) {
        out << make(Level::Problem, QObject::tr("Your account isn't allowed to add or change network connections"),
                    QObject::tr("Burrow can't create connections without this permission."),
                    QObject::tr("Ask the person who manages this computer to allow it, or add your user to the group that manages the network (often \"wheel\" or \"netdev\")."));
    } else if (in.mayModifyConnections == QLatin1String("auth")) {
        out << make(Level::Info, QObject::tr("You'll be asked for your password when changing connections"),
                    QObject::tr("This is normal on some systems. Burrow can't continue until you enter it."));
    }
    if (in.mayControlNetwork == QLatin1String("no")) {
        out << make(Level::Problem, QObject::tr("Your account isn't allowed to turn connections on or off"),
                    QObject::tr("You can create connections, but connecting them needs the \"network control\" permission."),
                    QObject::tr("Ask the person who manages this computer to allow it."));
    }

    const bool vpnUp = in.haveConnection && in.state == Tunnel::State::Connected;

    if (in.nmState > 0 && in.nmState <= 20 && !vpnUp) {
        out << make(Level::Problem, QObject::tr("This computer isn't connected to a network"),
                    QObject::tr("A VPN needs a working internet connection to build on."),
                    QObject::tr("Connect to Wi-Fi or plug in a network cable, then try again."));
    } else if (in.nmState == 40 || in.nmState == 30) {
        out << make(Level::Info, QObject::tr("Your network is still connecting"),
                    QObject::tr("Wait a few seconds, then run the checks again."));
    } else {
        switch (in.connectivity) {
        case 4:
            out << make(Level::Ok, vpnUp ? QObject::tr("The internet works through the VPN") : QObject::tr("The internet connection works"));
            break;
        case 3:
            out << make(Level::Warning, QObject::tr("Connected to a network, but there is no internet"),
                        QObject::tr("Your computer is on a network, but it can't reach the internet."),
                        vpnUp ? QObject::tr("The VPN may not be passing traffic. See the connection checks below.")
                              : QObject::tr("Check your router or Wi-Fi. The VPN can't work without internet."));
            break;
        case 2:
            out << make(Level::Warning, QObject::tr("This network wants you to sign in first"),
                        QObject::tr("Hotels, cafés and airports often show a sign-in page before they allow internet."),
                        QObject::tr("Open your web browser and complete the sign-in page, then connect again."));
            break;
        case 1:
            out << make(Level::Problem, QObject::tr("There is no internet connection"),
                        QObject::tr("NetworkManager can't reach the internet."),
                        QObject::tr("Check your router, Wi-Fi or cable."));
            break;
        default:
            out << make(Level::Info, QObject::tr("Internet check is turned off"),
                        QObject::tr("NetworkManager's connectivity check isn't enabled, so I can't tell whether the internet is reachable."));
        }
    }

    if (!in.kernelVersion.isEmpty()) {
        if (in.wireguardModuleLoaded || Diagnostics::kernelHasWireGuard(in.kernelVersion)) {
            out << make(Level::Ok, QObject::tr("This system supports WireGuard"));
        } else {
            out << make(Level::Problem, QObject::tr("Your Linux kernel is too old for WireGuard"),
                        QObject::tr("WireGuard is built into Linux 5.6 and newer. You are running %1.").arg(in.kernelVersion),
                        QObject::tr("Update your system, or install your distribution's WireGuard kernel module package."));
        }
    }
}

void configChecks(const Diagnostics::Input &in, QList<Finding> &out)
{
    const WgConfig &c = in.config;
    const int before = int(out.size());

    if (c.addresses.isEmpty()) {
        out << make(Level::Problem, QObject::tr("This connection has no VPN address"),
                    QObject::tr("Your device needs an address inside the VPN, which your provider gives you."),
                    QObject::tr("Enter it (it looks like 10.2.0.2/32) or import the file from your provider again."));
    }
    if (c.peers.isEmpty()) {
        out << make(Level::Problem, QObject::tr("This connection has no server"),
                    QObject::tr("There is nothing to connect to."),
                    QObject::tr("Import the file from your provider again, or create the connection with the server's details."));
    }

    for (int i = 0; i < c.peers.size(); ++i) {
        const WgPeer &p = c.peers[i];
        const QString who = c.peers.size() > 1 ? QObject::tr(" (server %1)").arg(i + 1) : QString();

        if (!ConfParser::isValidKey(p.publicKey)) {
            out << make(Level::Problem, QObject::tr("The server's public key isn't valid") + who,
                        QObject::tr("A WireGuard key is 44 characters long and ends with an equals sign."),
                        QObject::tr("Copy the server's public key again from your provider, without extra spaces."));
        }
        if (p.endpoint.isEmpty()) {
            out << make(Level::Warning, QObject::tr("There is no server address") + who,
                        QObject::tr("Without an address, this device can't start the connection."),
                        QObject::tr("Add the server address, for example vpn.example.com:51820."));
        }
        if (p.allowedIps.isEmpty()) {
            out << make(Level::Problem, QObject::tr("No traffic is set to use the VPN") + who,
                        QObject::tr("The \"traffic to send\" list is empty, so nothing would go through the tunnel."),
                        QObject::tr("Use 0.0.0.0/0, ::/0 to send everything through the VPN."));
        }
    }

    if (c.mtu && c.mtu < 1280) {
        out << make(Level::Problem, QObject::tr("The MTU setting is too small"),
                    QObject::tr("MTU is %1. Values under 1280 break IPv6 and can stall connections.").arg(c.mtu),
                    QObject::tr("Remove the MTU setting, or use a value around 1420."));
    } else if (c.mtu > 1500) {
        out << make(Level::Warning, QObject::tr("The MTU setting is unusually large"),
                    QObject::tr("MTU is %1. Large values can make pages stall or load partly.").arg(c.mtu),
                    QObject::tr("Remove the MTU setting, or use a value around 1420."));
    }

    const bool fullTunnel = std::any_of(c.peers.cbegin(), c.peers.cend(), [](const WgPeer &p) { return p.allowedIps.contains("0.0.0.0/0"); });
    const bool fullTunnel6 = std::any_of(c.peers.cbegin(), c.peers.cend(), [](const WgPeer &p) { return p.allowedIps.contains("::/0"); });

    if (fullTunnel && c.dnsServers.isEmpty()) {
        out << make(Level::Warning, QObject::tr("No DNS server is set"),
                    QObject::tr("All your traffic goes through the VPN, but there's no DNS server for it. Websites may not load by name, or the lookups may leave the VPN."),
                    QObject::tr("Add the DNS address from your provider, or a public one such as 1.1.1.1."));
    }
    if (fullTunnel && !fullTunnel6) {
        out << make(Level::Info, QObject::tr("IPv6 traffic doesn't use the VPN"),
                    QObject::tr("Sites that are reachable over IPv6 may be reached directly, not through the VPN."),
                    QObject::tr("Add ::/0 to the traffic list if your provider supports IPv6."));
    }

    if (out.size() == before)
        out << make(Level::Ok, QObject::tr("The connection settings look complete"));
}

void stateChecks(const Diagnostics::Input &in, QList<Finding> &out)
{
    constexpr qint64 kSlowConnectSeconds = 15;
    constexpr qint64 kNoReplySeconds = 10;

    switch (in.state) {
    case Tunnel::State::Connecting:
        if (in.secondsInState >= kSlowConnectSeconds) {
            out << make(Level::Warning, QObject::tr("It's taking a long time to connect"),
                        QObject::tr("This usually means the server can't be reached: the address or port is wrong, a firewall is blocking it, or the server is down."),
                        QObject::tr("Try a different network, such as your phone's hotspot. If it works there, your network blocks VPN traffic. "
                                    "Otherwise check the server address and port."));
        } else {
            out << make(Level::Info, QObject::tr("Connecting…"), QObject::tr("This normally takes a few seconds."));
        }
        break;

    case Tunnel::State::Disconnecting:
        out << make(Level::Info, QObject::tr("Disconnecting…"));
        break;

    case Tunnel::State::Disconnected:
        if (!in.lastFailure.isEmpty()) {
            out << make(Level::Problem, QObject::tr("The last attempt to connect failed"), in.lastFailure,
                        QObject::tr("Look at the other results for the likely cause, then try again."));
        } else {
            out << make(Level::Info, QObject::tr("Not connected"),
                        QObject::tr("Connect first, then run the checks again to test the live connection."));
        }
        break;

    case Tunnel::State::Connected:
        if (!in.countersOk) {
            out << make(Level::Info, QObject::tr("Couldn't read the traffic counters"),
                        QObject::tr("The connection is up, but I couldn't check whether data is flowing."));
        } else if (in.receivedBytes > 0) {
            out << make(Level::Ok, QObject::tr("Data is flowing both ways"),
                        QObject::tr("%1 sent, %2 received.").arg(bytes(in.sentBytes), bytes(in.receivedBytes)));
        } else if (in.sentBytes > 0 && in.secondsInState >= kNoReplySeconds) {
            out << make(Level::Problem, QObject::tr("Your computer is sending data, but the server isn't answering"),
                        QObject::tr("The tunnel is up on this side, but nothing has come back from the server."),
                        QObject::tr("The usual causes, most likely first:\n"
                                    "• The server doesn't know this device's public key yet. Add it in your provider's or server's settings.\n"
                                    "• The server's public key or address is wrong.\n"
                                    "• A firewall or network is blocking the connection. Try another network to rule this out."));
        } else if (in.sentBytes > 0) {
            out << make(Level::Info, QObject::tr("Waiting for the server to answer…"),
                        QObject::tr("Data has been sent. Give it a few seconds, then run the checks again."));
        } else if (in.secondsInState >= kNoReplySeconds) {
            out << make(Level::Info, QObject::tr("No data has used the VPN yet"),
                        QObject::tr("An idle connection shows nothing. Open a website, then run the checks again."));
        }

        // is the DNS server this VPN asked for actually the one in use?
        if (!in.config.dnsServers.isEmpty() && !in.activeDns.isEmpty()) {
            const bool used = std::any_of(in.config.dnsServers.cbegin(), in.config.dnsServers.cend(),
                                          [&in](const QString &s) { return in.activeDns.contains(s); });
            if (!used) {
                out << make(Level::Warning, QObject::tr("The VPN's DNS server isn't being used"),
                            QObject::tr("Name lookups are going to a different server, so they may fail or bypass the VPN."),
                            QObject::tr("Disconnect and connect again. If it persists, your system's DNS settings may be overriding the VPN."));
            }
        }
        break;
    }
}

}

QList<Finding> Diagnostics::run(const Input &in)
{
    QList<Finding> out;
    if (!in.nmRunning) {
        out << make(Level::Problem, QObject::tr("NetworkManager isn't running"),
                    QObject::tr("Burrow uses NetworkManager to create and manage VPN connections, and can't do anything without it."),
                    QObject::tr("Start it with:  sudo systemctl enable --now NetworkManager\n"
                                "If your system uses a different network tool, Burrow can't manage connections there."));
        return out;
    }

    systemChecks(in, out);
    if (in.haveConnection) {
        configChecks(in, out);
        stateChecks(in, out);
    }
    return out;
}

Level Diagnostics::worstLevel(const QList<Finding> &findings)
{
    Level worst = Level::Ok;
    for (const Finding &f : findings)
        worst = std::max(worst, f.level);
    return worst;
}

QString Diagnostics::failureReasonText(uint reason)
{
    // NMActiveConnectionStateReason, https://networkmanager.dev/docs/api/latest/nm-dbus-types.html
    switch (reason) {
    case 3: return QObject::tr("The network connection it depends on went away.");
    case 4: return QObject::tr("NetworkManager (or its VPN service) stopped.");
    case 5: return QObject::tr("The address or DNS settings were not accepted.");
    case 6: return QObject::tr("It timed out. The server didn't respond in time.");
    case 7: return QObject::tr("The VPN service took too long to start.");
    case 8: return QObject::tr("The VPN service couldn't start.");
    case 9: return QObject::tr("The private key is missing, so the connection can't start.");
    case 10: return QObject::tr("The server refused the login.");
    case 12: return QObject::tr("Something this connection depends on failed to start.");
    case 13: return QObject::tr("Couldn't create the VPN network interface. Your kernel may not support WireGuard.");
    case 14: return QObject::tr("The VPN network interface was removed.");
    default: return {}; // unknown, none, user disconnected, connection removed: not failures
    }
}

bool Diagnostics::versionAtLeast(const QString &version, int major, int minor)
{
    const QStringList parts = version.split('.');
    bool okMajor = false, okMinor = false;
    const int vMajor = parts.value(0).toInt(&okMajor);
    const int vMinor = parts.value(1).toInt(&okMinor);
    if (!okMajor || !okMinor)
        return true; // can't tell: don't raise a false alarm
    return vMajor > major || (vMajor == major && vMinor >= minor);
}

bool Diagnostics::kernelHasWireGuard(const QString &kernelVersion)
{
    const QStringList parts = kernelVersion.split(QRegularExpression(QStringLiteral("[.\\-+]")));
    if (parts.size() < 2)
        return false;
    bool okMajor = false, okMinor = false;
    const int major = parts[0].toInt(&okMajor);
    const int minor = parts[1].toInt(&okMinor);
    return okMajor && okMinor && (major > 5 || (major == 5 && minor >= 6));
}

QString Diagnostics::redactEndpoint(const QString &endpoint)
{
    if (endpoint.isEmpty())
        return {};
    const QString host = hostOf(endpoint);
    const QString port = portOf(endpoint);

    QHostAddress addr;
    if (addr.setAddress(host)) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
            QStringList o = addr.toString().split('.');
            o.last() = QStringLiteral("x");
            return o.join('.') + ':' + port;
        }
        // all 8 groups, hide the last 4 (the part that identifies the machine), keep the network prefix
        const Q_IPV6ADDR bytes = addr.toIPv6Address();
        QStringList g;
        for (int i = 0; i < 8; ++i) {
            g << (i < 4 ? QString::number((quint16(bytes[2 * i]) << 8) | bytes[2 * i + 1], 16) : QStringLiteral("x"));
        }
        return '[' + g.join(':') + "]:" + port;
    }
    // a host name identifies the provider: keep only that it is a name
    return QObject::tr("<server name>:%1").arg(port);
}

QString Diagnostics::report(const Input &in, const QList<Finding> &findings)
{
    const auto levelName = [](Level l) {
        switch (l) {
        case Level::Ok: return QStringLiteral("OK");
        case Level::Info: return QStringLiteral("INFO");
        case Level::Warning: return QStringLiteral("WARNING");
        case Level::Problem: return QStringLiteral("PROBLEM");
        }
        return QString();
    };
    const auto stateName = [](Tunnel::State s) {
        switch (s) {
        case Tunnel::State::Connected: return QStringLiteral("connected");
        case Tunnel::State::Connecting: return QStringLiteral("connecting");
        case Tunnel::State::Disconnecting: return QStringLiteral("disconnecting");
        case Tunnel::State::Disconnected: return QStringLiteral("disconnected");
        }
        return QString();
    };

    QString r;
    r += QStringLiteral("Burrow diagnostic report\n========================\n");
    r += QStringLiteral("App:             %1%2\n").arg(in.appVersion, in.inFlatpak ? QStringLiteral(" (Flatpak)") : QString());
    r += QStringLiteral("System:          %1, kernel %2\n").arg(in.osName, in.kernelVersion);
    r += QStringLiteral("NetworkManager:  %1 (running: %2, state %3, connectivity %4)\n")
             .arg(in.nmVersion.isEmpty() ? QStringLiteral("unknown") : in.nmVersion, in.nmRunning ? "yes" : "no")
             .arg(in.nmState).arg(in.connectivity);

    if (in.haveConnection) {
        const WgConfig &c = in.config;
        r += QStringLiteral("\nConnection\n----------\n");
        r += QStringLiteral("State:           %1").arg(stateName(in.state));
        if (in.state == Tunnel::State::Connected || in.state == Tunnel::State::Connecting)
            r += QStringLiteral(" for %1 s").arg(in.secondsInState);
        r += '\n';
        if (in.countersOk)
            r += QStringLiteral("Traffic:         %1 bytes sent, %2 bytes received\n").arg(in.sentBytes).arg(in.receivedBytes);
        if (!in.lastFailure.isEmpty())
            r += QStringLiteral("Last failure:    %1\n").arg(in.lastFailure);

        int v4 = 0, v6 = 0;
        for (const WgAddress &a : c.addresses)
            (a.isV6 ? v6 : v4)++;
        r += QStringLiteral("Addresses:       %1 IPv4, %2 IPv6\n").arg(v4).arg(v6);
        r += QStringLiteral("DNS servers:     %1 (%2 search domains)\n").arg(c.dnsServers.size()).arg(c.dnsSearch.size());
        if (c.mtu)
            r += QStringLiteral("MTU:             %1\n").arg(c.mtu);
        r += QStringLiteral("Servers:         %1\n").arg(c.peers.size());
        for (int i = 0; i < c.peers.size(); ++i) {
            const WgPeer &p = c.peers[i];
            r += QStringLiteral("  server %1: endpoint %2, allowed IPs %3, keepalive %4, preshared key: %5\n")
                     .arg(i + 1)
                     .arg(p.endpoint.isEmpty() ? QStringLiteral("(none)") : redactEndpoint(p.endpoint),
                          p.allowedIps.join(QStringLiteral(", ")))
                     .arg(p.persistentKeepalive)
                     .arg(p.presharedKey.isEmpty() ? "no" : "yes");
        }
    }

    r += QStringLiteral("\nResults\n-------\n");
    for (const Finding &f : findings) {
        r += QStringLiteral("[%1] %2\n").arg(levelName(f.level), f.title);
        if (!f.detail.isEmpty())
            r += QStringLiteral("      %1\n").arg(QString(f.detail).replace('\n', QStringLiteral("\n      ")));
        if (!f.fix.isEmpty())
            r += QStringLiteral("      What to try: %1\n").arg(QString(f.fix).replace('\n', QStringLiteral("\n      ")));
    }
    r += QStringLiteral("\nThis report contains no keys, connection names or full server addresses.\n");
    return r;
}
