#pragma once

#include "core/Preferences.h"

#include <QDialog>

class QCheckBox;
class QComboBox;

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(Preferences &prefs, QWidget *parent = nullptr);

private:
    Preferences &m_prefs;
    Preferences::Theme m_original;
    QComboBox *m_theme;
    QComboBox *m_layout;
    QCheckBox *m_tray;
};
