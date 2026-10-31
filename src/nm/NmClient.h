#pragma once

#include "nm/Tunnel.h"

#include <QDBusConnection>
#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QVariantMap>

using NmSettings = QMap<QString, QVariantMap>;
Q_DECLARE_METATYPE(NmSettings)

class ActiveWatcher;

// What the troubleshooting checks need to know about the system.
struct NmSystemInfo {
    bool running = false;
    QString version;
    int state = 0;        // NMState
    int connectivity = 0; // NMConnectivityState
    QString mayModifyConnections;
    QString mayControlNetwork;
    QStringList dnsServers; // in use right now
};

// Talks to NetworkManager's Settings API over the system bus.
class NmClient : public QObject {
    Q_OBJECT
public:
    explicit NmClient(const QDBusConnection &bus = QDBusConnection::systemBus(), QObject *parent = nullptr);

    bool isAvailable() const;
    NmSystemInfo systemInfo() const;
    QList<Tunnel> tunnels() const { return m_tunnels; }

    // Creates a persistent connection; returns its path, or an empty path (and emits errorOccurred) on failure.
    QDBusObjectPath addConnection(const NmSettings &settings);

    // Removes the connection from NetworkManager (and with it the stored key). An active tunnel is brought down.
    void deleteTunnel(const QString &uuid);

    // GetSecrets("wireguard"): the private key and preshared keys, which GetSettings leaves out.
    // Returns false (and emits errorOccurred) when they cannot be read.
    bool getSecrets(const QString &uuid, NmSettings &secrets);

    void connectTunnel(const QString &uuid);
    void disconnectTunnel(const QString &uuid);

    // Maps NMActiveConnectionState to Tunnel::State (activating/deactivating/activated, else disconnected).
    static Tunnel::State stateFromNm(uint nmState);

    // Returns false when `settings` is not a WireGuard connection.
    static bool tunnelFromSettings(const QDBusObjectPath &path, const NmSettings &settings, Tunnel &out);

public slots:
    void refresh();
    void updateActive(); // re-reads which tunnels are active, without re-reading their settings

signals:
    void tunnelsChanged();
    void errorOccurred(const QString &message);

private:
    void watch(const QDBusObjectPath &activePath, const QString &uuid);
    void onActiveStateChanged(const QString &uuid, uint state, uint reason);

    QHash<QString, ActiveWatcher *> m_watchers; // by active-connection path
    QHash<QString, QString> m_failures;         // uuid -> why the last attempt failed
    QVariant getProperty(const QString &path, const char *iface, const char *name) const;
    void callNm(const char *method, const QVariantList &args);

    QDBusConnection m_bus;
    QList<Tunnel> m_tunnels;
};
