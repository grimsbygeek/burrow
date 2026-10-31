#include "core/KeyGen.h"

#include "core/ConfParser.h"

#include <sodium.h>

namespace {

bool initSodium()
{
    static const bool ok = sodium_init() >= 0;
    return ok;
}

QString toBase64(const unsigned char *bytes, size_t n)
{
    return QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(bytes), qsizetype(n)).toBase64());
}

}

KeyGen::KeyPair KeyGen::generate()
{
    if (!initSodium())
        return {};

    unsigned char priv[32];
    randombytes_buf(priv, sizeof priv);
    // Curve25519 clamping, same as `wg genkey`
    priv[0] &= 248;
    priv[31] &= 127;
    priv[31] |= 64;

    unsigned char pub[32];
    crypto_scalarmult_curve25519_base(pub, priv);

    const KeyPair pair{toBase64(priv, sizeof priv), toBase64(pub, sizeof pub)};
    sodium_memzero(priv, sizeof priv);
    return pair;
}

QString KeyGen::publicKeyFor(const QString &privateKey)
{
    if (!initSodium() || !ConfParser::isValidKey(privateKey))
        return {};

    QByteArray priv = QByteArray::fromBase64(privateKey.toLatin1());
    unsigned char pub[32];
    crypto_scalarmult_curve25519_base(pub, reinterpret_cast<const unsigned char *>(priv.constData()));
    sodium_memzero(priv.data(), size_t(priv.size()));
    return toBase64(pub, sizeof pub);
}

QString KeyGen::generatePresharedKey()
{
    if (!initSodium())
        return {};
    unsigned char psk[32];
    randombytes_buf(psk, sizeof psk);
    const QString out = toBase64(psk, sizeof psk);
    sodium_memzero(psk, sizeof psk);
    return out;
}
