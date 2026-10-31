#include "nm/NmClient.h"

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QHash>
#include <QSet>

#include "core/Diagnostics.h"
#include "nm/ActiveWatcher.h"
#include "nm/DBusUtil.h"

namespace {
constexpr auto kService = "org.freedesktop.NetworkManager";
constexpr auto kNmPath = "/org/freedesktop/NetworkManager";
constexpr auto kNmIface = "org.freedesktop.NetworkManager";
constexpr auto kActiveIface = "org.freedesktop.NetworkManager.Connection.Active";
constexpr auto kPropertiesIface = "org.freedesktop.DBus.Properties";
constexpr auto kSettingsPath = "/org/freedesktop/NetworkManager/Settings";
constexpr auto kSettingsIface = "org.freedesktop.NetworkManager.Settings";
constexpr auto kConnectionIface = "org.freedesktop.NetworkManager.Settings.Connection";
}

NmClient::NmClient(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
{
    qDBusRegisterMetaType<NmSettings>();
    qDBusRegisterMetaType<QList<QVariantMap>>();
    qDBusRegisterMetaType<QMap<QString, QString>>();

    for (const char *signal : {"NewConnection", "ConnectionRemoved"}) {
        m_bus.connect(kService, kSettingsPath, kSettingsIface, signal, this, SLOT(refresh()));
    }
    m_bus.connect(kService, kNmPath, kPropertiesIface, "PropertiesChanged", this, SLOT(updateActive()));
    // empty path: every active connection object
    m_bus.connect(kService, QString(), kActiveIface, "StateChanged", this, SLOT(updateActive()));
}

bool NmClient::isAvailable() const
{
    return m_bus.isConnected() && m_bus.interface()->isServiceRegistered(kService);
}

NmSystemInfo NmClient::systemInfo() const
{
    NmSystemInfo info;
    info.running = isAvailable();
    if (!info.running)
        return info;

    info.version = getProperty(kNmPath, kNmIface, "Version").toString();
    info.state = getProperty(kNmPath, kNmIface, "State").toInt();
    info.connectivity = getProperty(kNmPath, kNmIface, "Connectivity").toInt();

    // yes / auth (asks for a password) / no
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kNmPath, kNmIface, "GetPermissions");
    const QDBusReply<QMap<QString, QString>> perms = m_bus.call(call);
    if (perms.isValid()) {
        info.mayModifyConnections = perms.value().value("org.freedesktop.NetworkManager.settings.modify.system");
        info.mayControlNetwork = perms.value().value("org.freedesktop.NetworkManager.network-control");
    }

    const QVariant dns = getProperty("/org/freedesktop/NetworkManager/DnsManager", "org.freedesktop.NetworkManager.DnsManager", "Configuration");
    for (const QVariantMap &entry : unwrap<QList<QVariantMap>>(dns)) {
        for (const QString &server : unwrap<QStringList>(entry.value("nameservers"))) {
            if (!info.dnsServers.contains(server))
                info.dnsServers << server;
        }
    }
    return info;
}

bool NmClient::tunnelFromSettings(const QDBusObjectPath &path, const NmSettings &settings, Tunnel &out)
{
    const QVariantMap connection = settings.value("connection");
    if (connection.value("type").toString() != QLatin1String("wireguard"))
        return false;

    out.path = path;
    out.id = connection.value("id").toString();
    out.uuid = connection.value("uuid").toString();
    out.interfaceName = connection.value("interface-name").toString();
    out.autoconnect = connection.value("autoconnect", true).toBool();
    out.settings = settings;
    return true;
}

QDBusObjectPath NmClient::addConnection(const NmSettings &settings)
{
    QDBusInterface iface(kService, kSettingsPath, kSettingsIface, m_bus);
    const QDBusReply<QDBusObjectPath> reply = iface.call("AddConnection", QVariant::fromValue(settings));
    if (!reply.isValid()) {
        emit errorOccurred(reply.error().message());
        return {};
    }
    refresh();
    return reply.value();
}

void NmClient::refresh()
{
    if (!isAvailable()) {
        emit errorOccurred(tr("NetworkManager is not running"));
        return;
    }

    QDBusInterface settings(kService, kSettingsPath, kSettingsIface, m_bus);
    const QDBusReply<QList<QDBusObjectPath>> paths = settings.call("ListConnections");
    if (!paths.isValid()) {
        emit errorOccurred(paths.error().message());
        return;
    }

    QList<Tunnel> found;
    for (const QDBusObjectPath &path : paths.value()) {
        QDBusInterface conn(kService, path.path(), kConnectionIface, m_bus);
        const QDBusReply<NmSettings> reply = conn.call("GetSettings");
        Tunnel tunnel;
        if (reply.isValid() && tunnelFromSettings(path, reply.value(), tunnel))
            found.append(tunnel);
    }

    m_tunnels = found;
    updateActive();
}

Tunnel::State NmClient::stateFromNm(uint nmState)
{
    switch (nmState) {
    case 1: return Tunnel::State::Connecting;
    case 2: return Tunnel::State::Connected;
    case 3: return Tunnel::State::Disconnecting;
    default: return Tunnel::State::Disconnected;
    }
}

QVariant NmClient::getProperty(const QString &path, const char *iface, const char *name) const
{
    // Properties.Get directly: QDBusInterface::property() needs the property in introspection data.
    QDBusMessage call = QDBusMessage::createMethodCall(kService, path, kPropertiesIface, "Get");
    call << QString::fromLatin1(iface) << QString::fromLatin1(name);
    const QDBusMessage reply = m_bus.call(call);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return {};
    return reply.arguments().first().value<QDBusVariant>().variant();
}

void NmClient::updateActive()
{
    struct Active { Tunnel::State state; QDBusObjectPath path; };
    QHash<QString, Active> active;

    for (const QDBusObjectPath &path : unwrap<QList<QDBusObjectPath>>(getProperty(kNmPath, kNmIface, "ActiveConnections"))) {
        const QString uuid = getProperty(path.path(), kActiveIface, "Uuid").toString();
        if (!uuid.isEmpty())
            active.insert(uuid, {stateFromNm(getProperty(path.path(), kActiveIface, "State").toUInt()), path});
    }

    // watch every active connection for the reason it ends (also ones started outside this app)
    QSet<QString> livePaths;
    for (auto it = active.cbegin(); it != active.cend(); ++it) {
        livePaths.insert(it->path.path());
        watch(it->path, it.key());
    }
    for (auto it = m_watchers.begin(); it != m_watchers.end();) {
        if (livePaths.contains(it.key())) {
            ++it;
        } else {
            it.value()->deleteLater();
            it = m_watchers.erase(it);
        }
    }

    for (Tunnel &t : m_tunnels) {
        t.failure = m_failures.value(t.uuid);
        const auto it = active.constFind(t.uuid);
        t.state = it == active.constEnd() ? Tunnel::State::Disconnected : it->state;
        t.activePath = it == active.constEnd() ? QDBusObjectPath() : it->path;
    }
    emit tunnelsChanged();
}

void NmClient::watch(const QDBusObjectPath &activePath, const QString &uuid)
{
    if (m_watchers.contains(activePath.path()))
        return;
    auto *watcher = new ActiveWatcher(uuid, this);
    m_watchers.insert(activePath.path(), watcher);
    m_bus.connect(kService, activePath.path(), kActiveIface, "StateChanged", watcher, SLOT(onStateChanged(uint,uint)));
    connect(watcher, &ActiveWatcher::stateChanged, this, &NmClient::onActiveStateChanged);
}

void NmClient::onActiveStateChanged(const QString &uuid, uint state, uint reason)
{
    if (state == 4) { // deactivated: remember why, if it was a failure
        const QString why = Diagnostics::failureReasonText(reason);
        if (!why.isEmpty())
            m_failures.insert(uuid, why);
    } else if (state == 2) { // activated
        m_failures.remove(uuid);
    }
    updateActive();
}

void NmClient::callNm(const char *method, const QVariantList &args)
{
    QDBusInterface nm(kService, kNmPath, kNmIface, m_bus);
    auto *watcher = new QDBusPendingCallWatcher(nm.asyncCallWithArgumentList(method, args), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusPendingReply<> reply = *watcher;
        if (reply.isError())
            emit errorOccurred(reply.error().message());
        watcher->deleteLater();
        updateActive();
    });
}

void NmClient::deleteTunnel(const QString &uuid)
{
    for (const Tunnel &t : m_tunnels) {
        if (t.uuid != uuid)
            continue;
        QDBusInterface conn(kService, t.path.path(), kConnectionIface, m_bus);
        auto *watcher = new QDBusPendingCallWatcher(conn.asyncCall("Delete"), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
            const QDBusPendingReply<> reply = *watcher;
            if (reply.isError())
                emit errorOccurred(reply.error().message());
            watcher->deleteLater();
            refresh(); // also covers the case where the ConnectionRemoved signal was missed
        });
        return;
    }
}

bool NmClient::getSecrets(const QString &uuid, NmSettings &secrets)
{
    for (const Tunnel &t : m_tunnels) {
        if (t.uuid != uuid)
            continue;
        QDBusInterface conn(kService, t.path.path(), kConnectionIface, m_bus);
        const QDBusReply<NmSettings> reply = conn.call("GetSecrets", "wireguard");
        if (!reply.isValid()) {
            emit errorOccurred(reply.error().message());
            return false;
        }
        secrets = reply.value();
        return true;
    }
    return false;
}

void NmClient::connectTunnel(const QString &uuid)
{
    for (const Tunnel &t : m_tunnels) {
        if (t.uuid != uuid)
            continue;
        m_failures.remove(uuid); // a new attempt starts with a clean slate

        // "/" for device and specific object: let NetworkManager pick (WireGuard creates its own device)
        QDBusInterface nm(kService, kNmPath, kNmIface, m_bus);
        auto *watcher = new QDBusPendingCallWatcher(
            nm.asyncCall("ActivateConnection", QVariant::fromValue(t.path), QVariant::fromValue(QDBusObjectPath("/")),
                         QVariant::fromValue(QDBusObjectPath("/"))),
            this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, uuid] {
            const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
            if (reply.isError())
                emit errorOccurred(reply.error().message());
            else
                watch(reply.value(), uuid); // listen from the start, so a quick failure is not missed
            watcher->deleteLater();
            updateActive();
        });
        return;
    }
}

void NmClient::disconnectTunnel(const QString &uuid)
{
    for (const Tunnel &t : m_tunnels) {
        if (t.uuid == uuid && !t.activePath.path().isEmpty()) {
            callNm("DeactivateConnection", {QVariant::fromValue(t.activePath)});
            return;
        }
    }
}
