#pragma once

#include <QString>
#include <QtGlobal>

namespace NetStats {

struct Counters {
    bool ok = false;
    quint64 rxBytes = 0; // received
    quint64 txBytes = 0; // sent
};

// Reads the kernel's byte counters for a network interface. They start at zero each time the
// interface is created, so for a WireGuard tunnel they are "this connection" totals.
// `sysRoot` is only overridden by tests.
Counters read(const QString &interface, const QString &sysRoot = QStringLiteral("/sys/class/net"));

// "1.4 MB", "820 KB", "0 bytes"
QString formatBytes(quint64 bytes);

}
