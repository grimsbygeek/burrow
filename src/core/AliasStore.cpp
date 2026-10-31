#include "core/AliasStore.h"

#include <QStandardPaths>

namespace {
QString defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/aliases.ini");
}
}

AliasStore::AliasStore()
    : AliasStore(defaultPath())
{
}

AliasStore::AliasStore(const QString &iniPath)
    : m_settings(iniPath, QSettings::IniFormat)
{
}

QString AliasStore::alias(const QString &uuid) const
{
    return m_settings.value("aliases/" + uuid).toString();
}

void AliasStore::setAlias(const QString &uuid, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        m_settings.remove("aliases/" + uuid);
    else
        m_settings.setValue("aliases/" + uuid, trimmed);
    m_settings.sync();
}
