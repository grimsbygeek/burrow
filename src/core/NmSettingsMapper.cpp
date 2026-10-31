#include "core/NmSettingsMapper.h"

#include "nm/DBusUtil.h"

#include <QUuid>

namespace {

QList<QVariantMap> addressData(const WgConfig &config, bool v6)
{
    QList<QVariantMap> out;
    for (const WgAddress &a : config.addresses) {
        if (a.isV6 == v6)
            out.append({{"address", a.ip}, {"prefix", quint32(a.prefix)}});
    }
    return out;
}

QStringList dnsFor(const WgConfig &config, bool v6)
{
    QStringList out;
    for (const QString &ip : config.dnsServers) {
        if (ip.contains(':') == v6)
            out << ip;
    }
    return out;
}

QVariantMap ipSettings(const WgConfig &config, bool v6)
{
    const QList<QVariantMap> addrs = addressData(config, v6);
    QVariantMap ip;
    ip["method"] = addrs.isEmpty() ? (v6 ? "ignore" : "disabled") : "manual";
    if (!addrs.isEmpty())
        ip["address-data"] = QVariant::fromValue(addrs);

    const QStringList dns = dnsFor(config, v6);
    if (!dns.isEmpty()) {
        ip["dns-data"] = dns;
        if (!config.dnsSearch.isEmpty())
            ip["dns-search"] = config.dnsSearch;
    }
    return ip;
}

}

QString NmSettingsMapper::sanitizeInterfaceName(const QString &name)
{
    QString out;
    for (const QChar c : name) {
        if (c.isLetterOrNumber() || c == '_' || c == '-' || c == '=' || c == '+' || c == '.')
            out += c;
        else
            out += '_';
    }
    out.truncate(15);
    return out.isEmpty() || out == QLatin1String(".") || out == QLatin1String("..") ? QStringLiteral("wg0") : out;
}

QString NmSettingsMapper::uniqueInterfaceName(const QString &name, const QStringList &taken)
{
    const QString base = sanitizeInterfaceName(name);
    const auto isTaken = [&](const QString &candidate) { return taken.contains(candidate, Qt::CaseSensitive); };
    if (!isTaken(base))
        return base;
    for (int n = 2;; ++n) {
        const QString suffix = QStringLiteral("-%1").arg(n);
        const QString candidate = base.left(15 - suffix.size()) + suffix;
        if (!isTaken(candidate))
            return candidate;
    }
}

NmSettings NmSettingsMapper::toNmSettings(const WgConfig &config, const QString &id, const QString &interfaceName)
{
    NmSettings s;

    s["connection"] = {
        {"id", id},
        {"type", "wireguard"},
        {"uuid", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"interface-name", interfaceName},
        {"autoconnect", false},
    };

    QVariantMap wg;
    wg["private-key"] = config.privateKey;
    if (config.listenPort)
        wg["listen-port"] = quint32(config.listenPort);
    if (config.mtu)
        wg["mtu"] = quint32(config.mtu);
    if (config.fwmark)
        wg["fwmark"] = config.fwmark;

    QList<QVariantMap> peers;
    for (const WgPeer &p : config.peers) {
        QVariantMap peer;
        peer["public-key"] = p.publicKey;
        if (!p.presharedKey.isEmpty()) {
            peer["preshared-key"] = p.presharedKey;
            peer["preshared-key-flags"] = quint32(0);
        }
        if (!p.endpoint.isEmpty())
            peer["endpoint"] = p.endpoint;
        if (!p.allowedIps.isEmpty())
            peer["allowed-ips"] = p.allowedIps;
        if (p.persistentKeepalive)
            peer["persistent-keepalive"] = quint32(p.persistentKeepalive);
        peers.append(peer);
    }
    if (!peers.isEmpty())
        wg["peers"] = QVariant::fromValue(peers);
    s["wireguard"] = wg;

    s["ipv4"] = ipSettings(config, false);
    s["ipv6"] = ipSettings(config, true);
    return s;
}

WgConfig NmSettingsMapper::fromNmSettings(const NmSettings &settings, const NmSettings &secrets)
{
    WgConfig c;

    for (const bool v6 : {false, true}) {
        const QVariantMap ip = settings.value(v6 ? "ipv6" : "ipv4");
        for (const QVariantMap &a : unwrap<QList<QVariantMap>>(ip.value("address-data")))
            c.addresses.append({a.value("address").toString(), int(a.value("prefix").toUInt()), v6});
        c.dnsServers << unwrap<QStringList>(ip.value("dns-data"));
        for (const QString &domain : unwrap<QStringList>(ip.value("dns-search"))) {
            if (!c.dnsSearch.contains(domain))
                c.dnsSearch << domain;
        }
    }

    const QVariantMap wg = settings.value("wireguard");
    c.listenPort = wg.value("listen-port").toInt();
    c.mtu = wg.value("mtu").toInt();
    c.fwmark = wg.value("fwmark").toUInt();
    c.privateKey = secrets.value("wireguard").value("private-key").toString();

    const QList<QVariantMap> secretPeers = unwrap<QList<QVariantMap>>(secrets.value("wireguard").value("peers"));
    for (const QVariantMap &p : unwrap<QList<QVariantMap>>(wg.value("peers"))) {
        WgPeer peer;
        peer.publicKey = p.value("public-key").toString();
        peer.endpoint = p.value("endpoint").toString();
        peer.allowedIps = unwrap<QStringList>(p.value("allowed-ips"));
        peer.persistentKeepalive = p.value("persistent-keepalive").toInt();
        peer.presharedKey = p.value("preshared-key").toString(); // present when stored in the connection itself
        for (const QVariantMap &s : secretPeers) {
            if (s.value("public-key").toString() == peer.publicKey && s.contains("preshared-key"))
                peer.presharedKey = s.value("preshared-key").toString();
        }
        c.peers.append(peer);
    }
    return c;
}
