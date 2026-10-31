#!/usr/bin/env python3
"""Minimal fake of the NetworkManager D-Bus API used by wgtray, for integration tests.

Run it on a private session bus (see run_integration.sh). A connection whose id is
"fail" is refused on activation, to exercise error reporting.
"""
import dbus
import dbus.mainloop.glib
import dbus.service
from gi.repository import GLib

NM = "org.freedesktop.NetworkManager"
NM_PATH = "/org/freedesktop/NetworkManager"
SETTINGS_PATH = NM_PATH + "/Settings"
ACTIVE_IFACE = NM + ".Connection.Active"
PROPS = "org.freedesktop.DBus.Properties"


class Connection(dbus.service.Object):
    def __init__(self, bus, path, settings, manager=None):
        super().__init__(bus, path)
        self.settings = settings
        self.manager = manager
        self.path = path

    @dbus.service.method(NM + ".Settings.Connection")
    def Delete(self):
        self.manager.connections.pop(self.path, None)
        self.remove_from_connection()
        self.manager.settings_obj.ConnectionRemoved(dbus.ObjectPath(self.path))

    @dbus.service.method(NM + ".Settings.Connection", out_signature="a{sa{sv}}")
    def GetSettings(self):
        """Like the real thing, without secrets (private key, preshared keys)."""
        out = {k: dict(v) for k, v in self.settings.items()}
        wg = out.get("wireguard")
        if wg is not None:
            wg.pop("private-key", None)
            if "peers" in wg:
                peers = []
                for p in wg["peers"]:
                    p = dict(p)
                    p.pop("preshared-key", None)
                    p.pop("preshared-key-flags", None)
                    peers.append(dbus.Dictionary(p, signature="sv"))
                wg["peers"] = dbus.Array(peers, signature="a{sv}")
        return out

    @dbus.service.method(NM + ".Settings.Connection", in_signature="s", out_signature="a{sa{sv}}")
    def GetSecrets(self, setting):
        wg = self.settings.get("wireguard", {})
        out = {}
        if "private-key" in wg:
            out["private-key"] = wg["private-key"]
        secret_peers = [dbus.Dictionary({"public-key": p["public-key"], "preshared-key": p["preshared-key"]}, signature="sv")
                        for p in wg.get("peers", []) if "preshared-key" in p]
        if secret_peers:
            out["peers"] = dbus.Array(secret_peers, signature="a{sv}")
        return {"wireguard": out} if out else {}


class ActiveConnection(dbus.service.Object):
    def __init__(self, bus, path, conn_path, uuid):
        super().__init__(bus, path)
        self.conn_path, self.uuid, self.state = conn_path, uuid, 1

    @dbus.service.method(PROPS, in_signature="ss", out_signature="v")
    def Get(self, iface, name):
        return {"Uuid": self.uuid, "State": dbus.UInt32(self.state),
                "Connection": dbus.ObjectPath(self.conn_path)}[name]

    @dbus.service.signal(ACTIVE_IFACE, signature="uu")
    def StateChanged(self, state, reason):
        pass

    def set_state(self, state, reason=0):
        self.state = state
        self.StateChanged(dbus.UInt32(state), dbus.UInt32(reason))
        return False


class Manager(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, NM_PATH)
        self.bus = bus
        self.settings_obj = Settings(bus, self)
        self.connections = {}
        self.active = {}  # active path -> ActiveConnection
        self.counter = 0

    @dbus.service.method(PROPS, in_signature="ss", out_signature="v")
    def Get(self, iface, name):
        if name == "ActiveConnections":
            return dbus.Array([dbus.ObjectPath(p) for p in self.active], signature="o")
        return {"Version": "1.56.1", "State": dbus.UInt32(70), "Connectivity": dbus.UInt32(4)}[name]

    @dbus.service.method(NM, out_signature="a{ss}")
    def GetPermissions(self):
        return {"org.freedesktop.NetworkManager.settings.modify.system": "yes",
                "org.freedesktop.NetworkManager.network-control": "auth"}

    @dbus.service.signal(PROPS, signature="sa{sv}as")
    def PropertiesChanged(self, iface, changed, invalidated):
        pass

    def changed(self):
        self.PropertiesChanged(NM, {}, ["ActiveConnections"])

    @dbus.service.method(NM, in_signature="ooo", out_signature="o")
    def ActivateConnection(self, conn, device, specific):
        settings = self.connections[str(conn)].settings
        if settings["connection"]["id"] == "fail":
            raise dbus.exceptions.DBusException("activation refused", name=NM + ".Failed")
        self.counter += 1
        path = "%s/ActiveConnection/%d" % (NM_PATH, self.counter)
        self.active[path] = ActiveConnection(self.bus, path, str(conn), settings["connection"]["uuid"])
        self.changed()
        if settings["connection"]["id"] == "timeout":
            GLib.timeout_add(100, self.fail_activation, path, 6)  # NM_ACTIVE_CONNECTION_STATE_REASON_CONNECT_TIMEOUT
        else:
            GLib.timeout_add(100, self.finish_activation, path)
        return dbus.ObjectPath(path)

    def fail_activation(self, path, reason):
        ac = self.active.pop(path, None)
        if ac:
            ac.set_state(4, reason)
            ac.remove_from_connection()
            self.changed()
        return False

    def finish_activation(self, path):
        if path in self.active:
            self.active[path].set_state(2)
        return False

    @dbus.service.method(NM, in_signature="o")
    def DeactivateConnection(self, path):
        ac = self.active.pop(str(path), None)
        if ac:
            ac.set_state(4)
            ac.remove_from_connection()
            self.changed()


class DnsManager(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, NM_PATH + "/DnsManager")

    @dbus.service.method(PROPS, in_signature="ss", out_signature="v")
    def Get(self, iface, name):
        assert name == "Configuration"
        entry = dbus.Dictionary({"nameservers": dbus.Array(["10.2.0.1", "192.168.1.1"], signature="s"),
                                 "interface": "wgtest", "priority": dbus.Int32(50), "vpn": dbus.Boolean(True)}, signature="sv")
        return dbus.Array([entry], signature="a{sv}")


class Settings(dbus.service.Object):
    def __init__(self, bus, manager):
        super().__init__(bus, SETTINGS_PATH)
        self.bus, self.manager, self.n = bus, manager, 0

    @dbus.service.method(NM + ".Settings", out_signature="ao")
    def ListConnections(self):
        return dbus.Array([dbus.ObjectPath(p) for p in self.manager.connections], signature="o")

    @dbus.service.method(NM + ".Settings", in_signature="a{sa{sv}}", out_signature="o")
    def AddConnection(self, settings):
        self.n += 1
        path = "%s/%d" % (SETTINGS_PATH, self.n)
        self.manager.connections[path] = Connection(self.bus, path, settings, self.manager)
        self.NewConnection(dbus.ObjectPath(path))
        return dbus.ObjectPath(path)

    @dbus.service.signal(NM + ".Settings", signature="o")
    def NewConnection(self, path):
        pass

    @dbus.service.signal(NM + ".Settings", signature="o")
    def ConnectionRemoved(self, path):
        pass


if __name__ == "__main__":
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    name = dbus.service.BusName(NM, bus)
    Manager(bus)
    DnsManager(bus)
    GLib.MainLoop().run()
