#include "core/Preferences.h"

#include <QStandardPaths>

namespace {
constexpr auto kTheme = "appearance/theme";
constexpr auto kTray = "behaviour/close_to_tray";
constexpr auto kSetup = "setup/first_run_done";
constexpr auto kSimple = "appearance/simple_layout";

QString defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/settings.ini");
}
}

Preferences::Preferences()
    : Preferences(defaultPath())
{
}

Preferences::Preferences(const QString &iniPath)
    : m_settings(iniPath, QSettings::IniFormat)
{
}

Preferences::Theme Preferences::theme() const
{
    const QString v = m_settings.value(kTheme).toString();
    if (v == QLatin1String("light"))
        return Theme::Light;
    if (v == QLatin1String("dark"))
        return Theme::Dark;
    return Theme::System;
}

void Preferences::setTheme(Theme theme)
{
    const char *v = theme == Theme::Light ? "light" : theme == Theme::Dark ? "dark" : "system";
    m_settings.setValue(kTheme, QString::fromLatin1(v));
    m_settings.sync();
}

bool Preferences::simpleLayout() const
{
    return m_settings.value(kSimple, true).toBool();
}

void Preferences::setSimpleLayout(bool simple)
{
    m_settings.setValue(kSimple, simple);
    m_settings.sync();
}

bool Preferences::closeToTray() const
{
    return m_settings.value(kTray, true).toBool();
}

void Preferences::setCloseToTray(bool on)
{
    m_settings.setValue(kTray, on);
    m_settings.sync();
}

bool Preferences::setupDone() const
{
    return m_settings.value(kSetup, false).toBool();
}

void Preferences::setSetupDone(bool done)
{
    m_settings.setValue(kSetup, done);
    m_settings.sync();
}
