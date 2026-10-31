#pragma once

#include "core/WgConfig.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QDialogButtonBox;

// "New connection": a name, then either a .conf file from the VPN provider or details typed in.
class NewConnectionDialog : public QDialog {
    Q_OBJECT
public:
    // `takenNames` are existing connection names (case-insensitive) that cannot be reused.
    explicit NewConnectionDialog(const QStringList &takenNames, QWidget *parent = nullptr);

    QString name() const;
    WgConfig config() const { return m_config; }

private:
    QWidget *buildImportTab();
    QWidget *buildManualTab();
    void chooseFile();
    void validate();

    QStringList m_taken;
    WgConfig m_config;
    bool m_configValid = false;
    bool m_ready = false; // false while the form is still being built: validate() would touch unbuilt widgets

    QLineEdit *m_name;
    QTabWidget *m_tabs;
    QLabel *m_error;
    QDialogButtonBox *m_buttons;

    // import tab
    QLabel *m_fileLabel = nullptr;
    QString m_importError;
    QStringList m_importWarnings;
    WgConfig m_imported;
    bool m_importOk = false;

    // manual tab
    QLineEdit *m_privateKey = nullptr;
    QLineEdit *m_publicKey = nullptr;
    QLineEdit *m_address = nullptr;
    QLineEdit *m_dns = nullptr;
    QLineEdit *m_serverKey = nullptr;
    QLineEdit *m_endpoint = nullptr;
    QLineEdit *m_allowedIps = nullptr;
    QLineEdit *m_presharedKey = nullptr;
    QLineEdit *m_keepalive = nullptr;
    QLineEdit *m_mtu = nullptr;
};
