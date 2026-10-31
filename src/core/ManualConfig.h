#pragma once

#include "core/ConfParser.h"

// What the "enter manually" form collects. Everything is free text as typed by the user.
struct ManualInput {
    QString privateKey;
    QString address;      // "10.0.0.2/24", comma-separated for several
    QString dns;          // optional
    QString serverPublicKey;
    QString endpoint;     // "vpn.example.com:51820"
    QString allowedIps;   // "0.0.0.0/0, ::/0"
    QString presharedKey; // optional
    QString keepalive;    // optional, seconds
    QString mtu;          // optional
};

namespace ManualConfig {

// Validates with the same rules as an imported file. Errors are worded for the form (no line numbers).
ParseResult build(const ManualInput &input);

}
