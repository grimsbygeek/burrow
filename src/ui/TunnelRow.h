#pragma once

#include "nm/Tunnel.h"

#include <QWidget>

// One entry in the tunnel list.
// Simple: the name and a status dot. Detailed: name, interface, and a Connect button.
class TunnelRow : public QWidget {
    Q_OBJECT
public:
    TunnelRow(const Tunnel &tunnel, bool simple, QWidget *parent = nullptr);

signals:
    void connectClicked(const QString &uuid);

private:
    QString m_uuid;
};
