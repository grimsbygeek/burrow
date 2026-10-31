#include "ui/TrayController.h"

#include "ui/Style.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>

QIcon TrayController::iconFor(bool connected)
{
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64}) {
        QPixmap pm(QStringLiteral(":/icons/wgtray-%1.png").arg(size));
        if (connected) {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            const qreal d = size * 0.46;
            const QRectF dot(size - d, size - d, d, d);
            p.setPen(QPen(Qt::white, qMax(1.0, size / 32.0)));
            p.setBrush(QColor(Style::dotGreen));
            p.drawEllipse(dot);
        }
        icon.addPixmap(pm);
    }
    return icon;
}

TrayController::TrayController(QObject *parent)
    : QObject(parent)
    , m_icon(new QSystemTrayIcon(this))
    , m_menu(new QMenu)
{
    m_menu->addAction(tr("Open Burrow"), this, &TrayController::openRequested);
    m_icon->setContextMenu(m_menu);

    connect(m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            emit toggleWindowRequested();
    });

    setTunnels({});
}

bool TrayController::isAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayController::show()
{
    m_icon->show();
}

void TrayController::notify(const QString &title, const QString &message)
{
    if (m_icon->isVisible())
        m_icon->showMessage(title, message, m_icon->icon(), 5000);
}

QString TrayController::tooltipFor(const QList<Tunnel> &tunnels)
{
    QStringList connected;
    for (const Tunnel &t : tunnels) {
        if (t.state == Tunnel::State::Connected)
            connected << t.label();
    }
    const QString title = tr("Burrow");
    return connected.isEmpty() ? tr("%1\nNot connected").arg(title) : tr("%1\nConnected: %2").arg(title, connected.join(", "));
}

void TrayController::setTunnels(const QList<Tunnel> &tunnels)
{
    // Rebuild everything after the first entry ("Open…") each time; there are only ever a few tunnels.
    const QList<QAction *> old = m_menu->actions();
    for (qsizetype i = 1; i < old.size(); ++i) {
        m_menu->removeAction(old[i]);
        old[i]->deleteLater();
    }

    m_menu->addSeparator();
    if (tunnels.isEmpty()) {
        m_menu->addAction(tr("No connections yet"))->setEnabled(false);
    }
    for (const Tunnel &t : tunnels) {
        const bool up = t.state == Tunnel::State::Connected;
        const bool busy = t.state == Tunnel::State::Connecting || t.state == Tunnel::State::Disconnecting;
        const QString text = busy ? (t.state == Tunnel::State::Connecting ? tr("%1 (connecting…)") : tr("%1 (disconnecting…)")).arg(t.label())
                             : up ? tr("Disconnect %1").arg(t.label())
                                  : tr("Connect %1").arg(t.label());
        QAction *action = m_menu->addAction(text);
        action->setEnabled(!busy);
        const QString uuid = t.uuid;
        connect(action, &QAction::triggered, this, [this, up, uuid] {
            if (up)
                emit disconnectRequested(uuid);
            else
                emit connectRequested(uuid);
        });
    }
    m_menu->addSeparator();
    m_menu->addAction(tr("Quit"), this, &TrayController::quitRequested);

    const bool anyConnected = std::any_of(tunnels.cbegin(), tunnels.cend(), [](const Tunnel &t) { return t.state == Tunnel::State::Connected; });
    if (!m_iconSet || anyConnected != m_anyConnected) {
        m_icon->setIcon(iconFor(anyConnected));
        m_anyConnected = anyConnected;
        m_iconSet = true;
    }
    m_icon->setToolTip(tooltipFor(tunnels));
}
