#pragma once

#include "nm/Tunnel.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

// Right-hand pane: details of the selected tunnel plus a Disconnect button.
class DetailPane : public QWidget {
    Q_OBJECT
public:
    explicit DetailPane(QWidget *parent = nullptr);

    void showTunnel(const Tunnel &tunnel);
    void clear(bool haveTunnels = true);
    void setSimple(bool simple);

    // Live traffic for the connection being shown; clearTraffic() shows dashes.
    void setTraffic(quint64 sentBytes, quint64 receivedBytes);
    void clearTraffic();

signals:
    void connectClicked(const QString &uuid);
    void importRequested();
    void exportRequested(const QString &uuid);
    void troubleshootRequested(const QString &uuid);
    void deleteRequested(const QString &uuid);
    void disconnectClicked(const QString &uuid);
    void renameRequested(const QString &uuid, const QString &name);

private:
    void applyMode();
    void startRename();
    void finishRename(bool accept);

    QStackedWidget *m_stack;
    QLabel *m_status;
    QLabel *m_title;
    QToolButton *m_editButton;
    QToolButton *m_exportButton;
    QToolButton *m_deleteButton;
    QLineEdit *m_nameEdit;
    QLabel *m_subtitle;
    QWidget *m_body;
    QPushButton *m_disconnect; // detailed layout
    QPushButton *m_action;     // simple layout: Connect or Disconnect, depending on state
    QToolButton *m_toggleDetails;
    QLabel *m_sentValue;
    QLabel *m_receivedValue;
    QLabel *m_summary; // server and address, one thin line
    QWidget *m_failureBar;
    QLabel *m_failureText;
    QLabel *m_emptyText;
    QPushButton *m_emptyImport;
    bool m_simple = true;
    bool m_detailsOpen = false;
    Tunnel::State m_state = Tunnel::State::Disconnected;
    QString m_uuid;
    QString m_currentName;
};
