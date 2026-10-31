#pragma once

#include <QDBusObjectPath>
#include <QMap>
#include <QString>
#include <QVariantMap>

struct Tunnel {
    enum class State { Disconnected, Connecting, Connected, Disconnecting };

    QDBusObjectPath path;
    QString id;          // NetworkManager's connection name
    QString displayName; // app-local rename; empty means use id
    QString uuid;
    QString interfaceName;
    bool autoconnect = true;
    State state = State::Disconnected;
    QDBusObjectPath activePath; // valid while the tunnel is active
    QString failure;            // plain-words reason the last attempt failed; cleared on the next attempt
    QMap<QString, QVariantMap> settings; // full GetSettings result (no secrets)

    QString label() const { return displayName.isEmpty() ? id : displayName; }
};
