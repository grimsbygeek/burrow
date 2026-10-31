#include "core/ManualConfig.h"

#include <QRegularExpression>
#include <algorithm>

namespace {

bool hasControlChars(const QString &s)
{
    return std::any_of(s.cbegin(), s.cend(), [](QChar c) { return c.unicode() < 0x20 || c.unicode() == 0x7f; });
}

}

ParseResult ManualConfig::build(const ManualInput &in)
{
    ParseResult result;

    const struct { const QString &value; const char *missing; } required[] = {
        {in.privateKey, "Enter your private key, or press Generate" },
        {in.address, "Enter the address your VPN gave you (for example 10.0.0.2/24)" },
        {in.serverPublicKey, "Enter the server's public key" },
        {in.endpoint, "Enter the server address (for example vpn.example.com:51820)" },
        {in.allowedIps, "Enter which traffic should use the VPN (usually 0.0.0.0/0, ::/0)" },
    };
    for (const auto &r : required) {
        if (r.value.trimmed().isEmpty())
            result.errors << QObject::tr(r.missing);
    }

    // A pasted value must never be able to smuggle extra lines (and so extra settings) into the config.
    for (const QString &v : {in.privateKey, in.address, in.dns, in.serverPublicKey, in.endpoint, in.allowedIps,
                             in.presharedKey, in.keepalive, in.mtu}) {
        if (hasControlChars(v)) {
            result.errors << QObject::tr("Remove the line breaks or special characters from what you entered");
            break;
        }
    }
    if (!result.errors.isEmpty())
        return result;

    QString text = QStringLiteral("[Interface]\nPrivateKey = %1\nAddress = %2\n").arg(in.privateKey.trimmed(), in.address.trimmed());
    if (!in.dns.trimmed().isEmpty())
        text += QStringLiteral("DNS = %1\n").arg(in.dns.trimmed());
    if (!in.mtu.trimmed().isEmpty())
        text += QStringLiteral("MTU = %1\n").arg(in.mtu.trimmed());

    text += QStringLiteral("\n[Peer]\nPublicKey = %1\nEndpoint = %2\nAllowedIPs = %3\n")
                .arg(in.serverPublicKey.trimmed(), in.endpoint.trimmed(), in.allowedIps.trimmed());
    if (!in.presharedKey.trimmed().isEmpty())
        text += QStringLiteral("PresharedKey = %1\n").arg(in.presharedKey.trimmed());
    if (!in.keepalive.trimmed().isEmpty())
        text += QStringLiteral("PersistentKeepalive = %1\n").arg(in.keepalive.trimmed());

    result = ConfParser::parse(text);

    // "Line 3: invalid Address 'x'" means nothing on a form
    static const QRegularExpression linePrefix(R"(^Line \d+: )");
    for (QString &e : result.errors)
        e.remove(linePrefix);
    for (QString &w : result.warnings)
        w.remove(linePrefix);
    return result;
}
