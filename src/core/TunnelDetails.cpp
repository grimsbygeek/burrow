#include "core/TunnelDetails.h"

#include "nm/DBusUtil.h"

TunnelDetails TunnelDetailsReader::fromSettings(const NmSettings &settings)
{
    TunnelDetails d;

    for (const char *family : {"ipv4", "ipv6"}) {
        const QVariantMap ip = settings.value(family);
        for (const QVariantMap &a : unwrap<QList<QVariantMap>>(ip.value("address-data")))
            d.addresses << QStringLiteral("%1/%2").arg(a.value("address").toString()).arg(a.value("prefix").toUInt());
        d.dns << unwrap<QStringList>(ip.value("dns-data"));
    }

    const QVariantMap wg = settings.value("wireguard");
    d.listenPort = wg.value("listen-port").toInt();
    d.mtu = wg.value("mtu").toInt();

    for (const QVariantMap &p : unwrap<QList<QVariantMap>>(wg.value("peers"))) {
        PeerDetails peer;
        peer.publicKey = p.value("public-key").toString();
        peer.endpoint = p.value("endpoint").toString();
        peer.allowedIps = unwrap<QStringList>(p.value("allowed-ips"));
        peer.persistentKeepalive = p.value("persistent-keepalive").toInt();
        d.peers << peer;
    }
    return d;
}
