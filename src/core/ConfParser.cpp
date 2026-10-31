#include "core/ConfParser.h"

#include <QHostAddress>
#include <QRegularExpression>

namespace {

enum class Section { None, Interface, Peer };

QStringList splitList(const QString &value)
{
    QStringList out;
    for (const QString &part : value.split(',', Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty())
            out << trimmed;
    }
    return out;
}

bool parseCidr(const QString &text, WgAddress &out, bool requirePrefix = false)
{
    const qsizetype slash = text.indexOf('/');
    const QString ip = slash < 0 ? text : text.left(slash);

    QHostAddress addr;
    if (!addr.setAddress(ip))
        return false;

    out.isV6 = addr.protocol() == QAbstractSocket::IPv6Protocol;
    out.ip = addr.toString();

    const int maxPrefix = out.isV6 ? 128 : 32;
    if (slash < 0) {
        if (requirePrefix)
            return false;
        out.prefix = maxPrefix;
        return true;
    }

    bool ok = false;
    out.prefix = text.mid(slash + 1).toInt(&ok);
    return ok && out.prefix >= 0 && out.prefix <= maxPrefix;
}

bool validEndpoint(const QString &endpoint)
{
    static const QRegularExpression re(R"(^(\[[0-9A-Fa-f:.]+\]|[^\s:\[\]]+):(\d{1,5})$)");
    const QRegularExpressionMatch m = re.match(endpoint);
    if (!m.hasMatch())
        return false;
    const int port = m.captured(2).toInt();
    return port >= 1 && port <= 65535;
}

bool parseUInt(const QString &value, qulonglong max, qulonglong &out)
{
    bool ok = false;
    // base 0 so that FwMark = 0x1234 works
    out = value.toULongLong(&ok, 0);
    return ok && out <= max;
}

}

bool ConfParser::isValidKey(const QString &base64Key)
{
    const QByteArray raw = base64Key.toLatin1();
    const QByteArray decoded = QByteArray::fromBase64(raw, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    return decoded.size() == 32 && decoded.toBase64() == raw;
}

ParseResult ConfParser::parse(const QString &text)
{
    ParseResult result;
    WgConfig &cfg = result.config;

    Section section = Section::None;
    bool sawInterface = false;
    int lineNo = 0;

    const auto error = [&](const QString &msg) { result.errors << QObject::tr("Line %1: %2").arg(lineNo).arg(msg); };
    const auto warn = [&](const QString &msg) { result.warnings << QObject::tr("Line %1: %2").arg(lineNo).arg(msg); };

    for (QString line : text.split('\n')) {
        ++lineNo;
        if (const qsizetype hash = line.indexOf('#'); hash >= 0)
            line.truncate(hash);
        line = line.trimmed();
        if (line.isEmpty())
            continue;

        if (line.startsWith('[')) {
            const QString name = line.toLower();
            if (name == QLatin1String("[interface]")) {
                if (sawInterface)
                    error(QObject::tr("duplicate [Interface] section"));
                sawInterface = true;
                section = Section::Interface;
            } else if (name == QLatin1String("[peer]")) {
                section = Section::Peer;
                cfg.peers.append(WgPeer{});
            } else {
                error(QObject::tr("unknown section %1").arg(line));
                section = Section::None;
            }
            continue;
        }

        const qsizetype eq = line.indexOf('=');
        if (eq < 0) {
            error(QObject::tr("expected 'Key = Value'"));
            continue;
        }
        const QString key = line.left(eq).trimmed().toLower();
        const QString value = line.mid(eq + 1).trimmed();

        if (section == Section::None) {
            error(QObject::tr("setting outside of any section"));
            continue;
        }

        if (section == Section::Interface) {
            qulonglong n = 0;
            if (key == QLatin1String("privatekey")) {
                if (!isValidKey(value))
                    error(QObject::tr("PrivateKey is not a valid WireGuard key"));
                cfg.privateKey = value;
            } else if (key == QLatin1String("address")) {
                for (const QString &item : splitList(value)) {
                    WgAddress a;
                    if (parseCidr(item, a))
                        cfg.addresses << a;
                    else
                        error(QObject::tr("invalid Address '%1'").arg(item));
                }
            } else if (key == QLatin1String("dns")) {
                for (const QString &item : splitList(value)) {
                    QHostAddress a;
                    if (a.setAddress(item))
                        cfg.dnsServers << a.toString();
                    else
                        cfg.dnsSearch << item;
                }
            } else if (key == QLatin1String("listenport")) {
                if (parseUInt(value, 65535, n))
                    cfg.listenPort = int(n);
                else
                    error(QObject::tr("invalid ListenPort"));
            } else if (key == QLatin1String("mtu")) {
                if (parseUInt(value, 65535, n))
                    cfg.mtu = int(n);
                else
                    error(QObject::tr("invalid MTU"));
            } else if (key == QLatin1String("fwmark")) {
                if (value.compare(QLatin1String("off"), Qt::CaseInsensitive) == 0)
                    cfg.fwmark = 0;
                else if (parseUInt(value, 0xFFFFFFFFu, n))
                    cfg.fwmark = quint32(n);
                else
                    error(QObject::tr("invalid FwMark"));
            } else if (key == QLatin1String("table") || key == QLatin1String("saveconfig")
                       || key == QLatin1String("preup") || key == QLatin1String("postup")
                       || key == QLatin1String("predown") || key == QLatin1String("postdown")) {
                warn(QObject::tr("'%1' is not supported and was ignored").arg(line.left(eq).trimmed()));
            } else {
                warn(QObject::tr("unknown setting '%1' ignored").arg(line.left(eq).trimmed()));
            }
            continue;
        }

        WgPeer &peer = cfg.peers.last();
        qulonglong n = 0;
        if (key == QLatin1String("publickey")) {
            if (!isValidKey(value))
                error(QObject::tr("PublicKey is not a valid WireGuard key"));
            peer.publicKey = value;
        } else if (key == QLatin1String("presharedkey")) {
            if (!isValidKey(value))
                error(QObject::tr("PresharedKey is not a valid WireGuard key"));
            peer.presharedKey = value;
        } else if (key == QLatin1String("allowedips")) {
            for (const QString &item : splitList(value)) {
                WgAddress a;
                if (parseCidr(item, a))
                    peer.allowedIps << QStringLiteral("%1/%2").arg(a.ip).arg(a.prefix);
                else
                    error(QObject::tr("invalid AllowedIPs entry '%1'").arg(item));
            }
        } else if (key == QLatin1String("endpoint")) {
            if (!validEndpoint(value))
                error(QObject::tr("invalid Endpoint '%1' (expected host:port)").arg(value));
            peer.endpoint = value;
        } else if (key == QLatin1String("persistentkeepalive")) {
            if (value.compare(QLatin1String("off"), Qt::CaseInsensitive) == 0)
                peer.persistentKeepalive = 0;
            else if (parseUInt(value, 65535, n))
                peer.persistentKeepalive = int(n);
            else
                error(QObject::tr("invalid PersistentKeepalive"));
        } else {
            warn(QObject::tr("unknown setting '%1' ignored").arg(line.left(eq).trimmed()));
        }
    }

    lineNo = 0;
    const auto missing = [&](const QString &msg) { result.errors << msg; };
    if (!sawInterface)
        missing(QObject::tr("missing [Interface] section"));
    else if (cfg.privateKey.isEmpty())
        missing(QObject::tr("[Interface] has no PrivateKey"));

    for (int i = 0; i < cfg.peers.size(); ++i) {
        if (cfg.peers[i].publicKey.isEmpty())
            missing(QObject::tr("Peer %1 has no PublicKey").arg(i + 1));
    }

    return result;
}
