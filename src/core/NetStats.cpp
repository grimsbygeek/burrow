#include "core/NetStats.h"

#include <QFile>
#include <QLocale>

namespace {

bool readNumber(const QString &path, quint64 &out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    bool ok = false;
    out = f.readAll().trimmed().toULongLong(&ok);
    return ok;
}

}

NetStats::Counters NetStats::read(const QString &interface, const QString &sysRoot)
{
    Counters c;
    // interface names come from imported files: never let one walk out of the stats directory
    if (interface.isEmpty() || interface.contains('/') || interface == QLatin1String(".") || interface == QLatin1String(".."))
        return c;

    const QString base = sysRoot + '/' + interface + "/statistics/";
    c.ok = readNumber(base + "rx_bytes", c.rxBytes) && readNumber(base + "tx_bytes", c.txBytes);
    return c;
}

QString NetStats::formatBytes(quint64 bytes)
{
    return QLocale().formattedDataSize(qint64(bytes), 1, QLocale::DataSizeTraditionalFormat);
}
