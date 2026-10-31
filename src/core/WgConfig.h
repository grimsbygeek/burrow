#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct WgAddress {
    QString ip;
    int prefix = 0;
    bool isV6 = false;
};

struct WgPeer {
    QString publicKey;
    QString presharedKey;
    QStringList allowedIps;
    QString endpoint;
    int persistentKeepalive = 0;
};

// In-memory form of a wg-quick .conf file.
struct WgConfig {
    QString privateKey;
    QList<WgAddress> addresses;
    QStringList dnsServers;
    QStringList dnsSearch;
    int listenPort = 0;
    int mtu = 0;
    quint32 fwmark = 0;
    QList<WgPeer> peers;
};
