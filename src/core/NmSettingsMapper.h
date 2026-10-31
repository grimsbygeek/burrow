#pragma once

#include "core/WgConfig.h"
#include "nm/NmClient.h"

namespace NmSettingsMapper {

// Builds the settings dictionary for Settings.AddConnection.
// `interfaceName` must already be a valid Linux interface name (see sanitizeInterfaceName).
NmSettings toNmSettings(const WgConfig &config, const QString &id, const QString &interfaceName);

// The reverse: rebuilds a config from a saved connection. `settings` is GetSettings (no secrets),
// `secrets` is GetSecrets("wireguard") - the private key and any preshared keys.
WgConfig fromNmSettings(const NmSettings &settings, const NmSettings &secrets);

// Derives a legal interface name (<= 15 chars, no '/' or whitespace) from a file/tunnel name.
QString sanitizeInterfaceName(const QString &name);

// sanitizeInterfaceName(name), made unique against `taken` by adding -2, -3, ... within the 15-char limit.
QString uniqueInterfaceName(const QString &name, const QStringList &taken);

}
