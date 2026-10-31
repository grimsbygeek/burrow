#pragma once

#include "nm/Tunnel.h"

#include <QIcon>
#include <QList>
#include <QObject>

class QMenu;
class QSystemTrayIcon;

// The system tray icon and its menu: open the window, connect/disconnect each tunnel, quit.
class TrayController : public QObject {
    Q_OBJECT
public:
    explicit TrayController(QObject *parent = nullptr);

    // False on desktops without a tray (for example GNOME without the AppIndicator extension).
    static bool isAvailable();

    void show();
    void setTunnels(const QList<Tunnel> &tunnels); // `label()` is what the menu shows
    void notify(const QString &title, const QString &message);

    QMenu *menu() const { return m_menu; }
    // The emblem, with a green dot in the corner when `connected`.
    static QIcon iconFor(bool connected);
    static QString tooltipFor(const QList<Tunnel> &tunnels);

signals:
    void toggleWindowRequested(); // click on the icon
    void openRequested();
    void connectRequested(const QString &uuid);
    void disconnectRequested(const QString &uuid);
    void quitRequested();

private:
    QSystemTrayIcon *m_icon;
    QMenu *m_menu;
    bool m_anyConnected = false;
    bool m_iconSet = false;
};
