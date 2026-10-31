#pragma once

#include "nm/NmClient.h"

#include <QStringList>

struct PeerDetails {
    QString publicKey;
    QString endpoint;
    QStringList allowedIps;
    int persistentKeepalive = 0;
};

// Read-only view of a NetworkManager WireGuard connection, for display.
struct TunnelDetails {
    QStringList addresses; // "ip/prefix", both families
    QStringList dns;
    int listenPort = 0;
    int mtu = 0;
    QList<PeerDetails> peers;
};

namespace TunnelDetailsReader {

TunnelDetails fromSettings(const NmSettings &settings);

}
