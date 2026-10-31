#pragma once

#include "core/WgConfig.h"

#include <QString>

namespace ConfWriter {

// wg-quick format: what ConfParser reads and the official WireGuard apps import.
QString toWgQuick(const WgConfig &config);

// Writes the text so that only the owner can read it (it contains a private key).
// Returns an empty string on success, otherwise a message describing the failure.
QString saveFile(const QString &path, const QString &text);

}
