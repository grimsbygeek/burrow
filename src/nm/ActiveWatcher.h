#pragma once

#include <QObject>

// Listens to one active connection's StateChanged(state, reason) signal and tags it with the
// connection's UUID, because the signal itself only says which D-Bus object it came from.
class ActiveWatcher : public QObject {
    Q_OBJECT
public:
    ActiveWatcher(const QString &uuid, QObject *parent) : QObject(parent), m_uuid(uuid) {}

public slots:
    void onStateChanged(uint state, uint reason) { emit stateChanged(m_uuid, state, reason); }

signals:
    void stateChanged(const QString &uuid, uint state, uint reason);

private:
    QString m_uuid;
};
