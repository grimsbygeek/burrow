#pragma once

#include "core/WgConfig.h"

struct ParseResult {
    WgConfig config;
    QStringList errors;   // the config must not be used when non-empty
    QStringList warnings; // ignored or unsupported settings

    bool ok() const { return errors.isEmpty(); }
};

namespace ConfParser {

ParseResult parse(const QString &text);

bool isValidKey(const QString &base64Key);

}
