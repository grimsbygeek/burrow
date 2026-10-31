#pragma once

#include <QSettings>
#include <QString>

// App-local display names for tunnels, keyed by NetworkManager connection UUID.
// Never written to NetworkManager.
class AliasStore {
public:
    // Default: the app's own config file (~/.config/wgtray/aliases.ini).
    AliasStore();
    explicit AliasStore(const QString &iniPath);

    QString alias(const QString &uuid) const;           // empty when none set
    void setAlias(const QString &uuid, const QString &name); // blank name clears the alias

private:
    mutable QSettings m_settings;
};
