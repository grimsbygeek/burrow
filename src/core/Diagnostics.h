#pragma once

#include "core/WgConfig.h"
#include "nm/Tunnel.h"

#include <QList>
#include <QString>
#include <QStringList>

// Plain-language checks for "why isn't my VPN working?". Pure logic: the caller gathers the facts
// (from NetworkManager, /sys, the saved connection) and this decides what they mean.
namespace Diagnostics {

struct Finding {
    enum class Level { Ok, Info, Warning, Problem };

    Level level = Level::Info;
    QString title;  // one line
    QString detail; // what it means, in everyday words
    QString fix;    // what to try; empty when there is nothing to do
};

struct Input {
    // the system
    bool nmRunning = false;
    QString nmVersion;
    int nmState = 0;       // NMState: 20 disconnected ... 70 connected to the internet
    int connectivity = 0;  // NMConnectivityState: 0 unknown, 1 none, 2 portal, 3 limited, 4 full
    QString kernelVersion; // for example "6.11.2-300.fc44.x86_64"
    bool wireguardModuleLoaded = false;
    QStringList activeDns; // DNS servers NetworkManager is using right now
    // What this user may do, from NetworkManager's GetPermissions: "yes", "auth" (asks for a password),
    // "no", or empty when unknown.
    QString mayModifyConnections; // org.freedesktop.NetworkManager.settings.modify.system
    QString mayControlNetwork;    // org.freedesktop.NetworkManager.network-control

    // the connection being examined
    bool haveConnection = false;
    WgConfig config;
    Tunnel::State state = Tunnel::State::Disconnected;
    qint64 secondsInState = 0; // how long it has been connecting / connected
    bool countersOk = false;
    quint64 sentBytes = 0;
    quint64 receivedBytes = 0;
    QString lastFailure; // text from failureReasonText(), empty when the last attempt did not fail

    // only used by report()
    QString appVersion;
    QString osName;
    bool inFlatpak = false;
};

QList<Finding> run(const Input &input);

// The worst level among the findings.
Finding::Level worstLevel(const QList<Finding> &findings);

// NetworkManager's reason code for an activation ending (NMActiveConnectionStateReason), in plain words.
// Empty when the end was not a failure (the user disconnected, or the connection was removed).
QString failureReasonText(uint nmReason);

// "1.56.1" >= 1.16 ?
bool versionAtLeast(const QString &version, int major, int minor);

// WireGuard is part of Linux from 5.6.
bool kernelHasWireGuard(const QString &kernelVersion);

// Hides the last part of an address so a report can be shared: "185.76.11.17:51820" -> "185.76.11.x:51820".
QString redactEndpoint(const QString &endpoint);

// Plain text for pasting into a forum post or bug report. Contains no keys, no connection names and no
// full server address.
QString report(const Input &input, const QList<Finding> &findings);

}
