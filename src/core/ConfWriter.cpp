#include "core/ConfWriter.h"

#include <QFile>

QString ConfWriter::toWgQuick(const WgConfig &c)
{
    QString out;
    out += QStringLiteral("[Interface]\n");
    out += QStringLiteral("PrivateKey = %1\n").arg(c.privateKey);

    QStringList addresses;
    for (const WgAddress &a : c.addresses)
        addresses << QStringLiteral("%1/%2").arg(a.ip).arg(a.prefix);
    if (!addresses.isEmpty())
        out += QStringLiteral("Address = %1\n").arg(addresses.join(QStringLiteral(", ")));

    if (!c.dnsServers.isEmpty() || !c.dnsSearch.isEmpty())
        out += QStringLiteral("DNS = %1\n").arg((c.dnsServers + c.dnsSearch).join(QStringLiteral(", ")));
    if (c.listenPort)
        out += QStringLiteral("ListenPort = %1\n").arg(c.listenPort);
    if (c.mtu)
        out += QStringLiteral("MTU = %1\n").arg(c.mtu);
    if (c.fwmark)
        out += QStringLiteral("FwMark = 0x%1\n").arg(c.fwmark, 0, 16);

    for (const WgPeer &p : c.peers) {
        out += QStringLiteral("\n[Peer]\n");
        out += QStringLiteral("PublicKey = %1\n").arg(p.publicKey);
        if (!p.presharedKey.isEmpty())
            out += QStringLiteral("PresharedKey = %1\n").arg(p.presharedKey);
        if (!p.allowedIps.isEmpty())
            out += QStringLiteral("AllowedIPs = %1\n").arg(p.allowedIps.join(QStringLiteral(", ")));
        if (!p.endpoint.isEmpty())
            out += QStringLiteral("Endpoint = %1\n").arg(p.endpoint);
        if (p.persistentKeepalive)
            out += QStringLiteral("PersistentKeepalive = %1\n").arg(p.persistentKeepalive);
    }
    return out;
}

QString ConfWriter::saveFile(const QString &path, const QString &text)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return f.errorString();
    // tighten permissions before any secret is written (also fixes an existing, looser file)
    if (!f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner))
        return QObject::tr("Could not restrict the file's permissions");
    if (f.write(text.toUtf8()) < 0)
        return f.errorString();
    f.close();
    return f.error() == QFileDevice::NoError ? QString() : f.errorString();
}
