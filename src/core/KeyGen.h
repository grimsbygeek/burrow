#pragma once

#include <QString>

namespace KeyGen {

struct KeyPair {
    QString privateKey; // base64, as `wg genkey` prints it
    QString publicKey;  // base64, as `wg pubkey` prints it
};

KeyPair generate();

// Empty when `privateKey` is not a valid WireGuard key.
QString publicKeyFor(const QString &privateKey);

// 32 random bytes, base64: what `wg genpsk` prints.
QString generatePresharedKey();

}
