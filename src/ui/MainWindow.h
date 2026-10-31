#pragma once

#include "core/AliasStore.h"
#include "core/Diagnostics.h"
#include "core/Preferences.h"
#include "nm/Tunnel.h"

#include <QDateTime>
#include <QHash>
#include <QMainWindow>

class DetailPane;
class TrayController;
class NmClient;
class QListWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void quit();
    void toggleWindow();
    void showWindow();
    QWidget *buildHeader();
    void rebuildList();
    void newConnection();
    void exportTunnel(const QString &uuid);
    void troubleshoot(const QString &uuid); // empty uuid: system checks only
    void trackStates();
    Diagnostics::Input gatherDiagnostics(const QString &uuid) const;
    void firstRunCheck();
    void deleteTunnel(const QString &uuid);
    void renameTunnel(const QString &uuid, const QString &name);
    QList<Tunnel> labelledTunnels() const; // NetworkManager tunnels with app-local names applied
    void showSelected();
    void updateTraffic();

    NmClient *m_nm;
    AliasStore m_aliases;
    Preferences m_prefs;
    QListWidget *m_list;
    DetailPane *m_detail;
    TrayController *m_tray = nullptr;
    bool m_trayHintShown = false;
    bool m_quitting = false; // quit() closes every window first; this lets closeEvent() let it through
    struct Seen {
        Tunnel::State state;
        QDateTime since;
    };
    QHash<QString, Seen> m_seen; // when each connection entered its current state
    bool m_startupCheckShown = false;
    QString m_selectedUuid; // survives list rebuilds
};
