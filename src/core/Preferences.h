#pragma once

#include <QSettings>
#include <QString>

// Persistent user preferences (~/.config/wgtray/settings.ini).
class Preferences {
public:
    enum class Theme { System, Light, Dark };

    Preferences();
    explicit Preferences(const QString &iniPath);

    Theme theme() const;
    void setTheme(Theme theme);

    // Simple (default): names, one big Connect/Disconnect button, details tucked away.
    // Detailed: per-row buttons and all technical fields visible.
    // Closing the window keeps the app running in the system tray (when the desktop has one).
    bool closeToTray() const;
    void setCloseToTray(bool on);

    // Whether the first-run setup check has been shown.
    bool setupDone() const;
    void setSetupDone(bool done);

    bool simpleLayout() const;
    void setSimpleLayout(bool simple);

private:
    mutable QSettings m_settings;
};
