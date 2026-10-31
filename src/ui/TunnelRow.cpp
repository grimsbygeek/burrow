#include "ui/TunnelRow.h"

#include "ui/Style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

TunnelRow::TunnelRow(const Tunnel &tunnel, bool simple, QWidget *parent)
    : QWidget(parent)
    , m_uuid(tunnel.uuid)
{
    auto *name = new QLabel(tunnel.label(), this);
    QFont bold = name->font();
    bold.setBold(true);
    name->setFont(bold);

    using State = Tunnel::State;

    if (simple) {
        QFont f = name->font();
        f.setPointSizeF(f.pointSizeF() * 1.15);
        name->setFont(f);

        auto *dot = new QLabel(this);
        if (tunnel.state == State::Connected) {
            dot->setText(QStringLiteral("●"));
            dot->setStyleSheet(QStringLiteral("color: %1; font-size: 16px;").arg(Style::dotGreen));
            dot->setToolTip(tr("Connected"));
        } else if (tunnel.state != State::Disconnected) {
            dot->setText(QStringLiteral("●"));
            dot->setStyleSheet(QStringLiteral("color: %1; font-size: 16px;").arg(Style::dotAmber));
            dot->setToolTip(tunnel.state == State::Connecting ? tr("Connecting…") : tr("Disconnecting…"));
        }

        auto *simpleLayout = new QHBoxLayout(this);
        simpleLayout->setContentsMargins(12, 12, 12, 12);
        simpleLayout->addWidget(name, 1);
        simpleLayout->addWidget(dot);
        return;
    }

    auto *iface = new QLabel(tunnel.interfaceName, this);
    iface->setEnabled(false); // renders in the palette's disabled (muted) color

    auto *text = new QVBoxLayout;
    text->setSpacing(0);
    text->addWidget(name);
    text->addWidget(iface);

    QWidget *action = nullptr;
    if (tunnel.state == State::Connected) {
        auto *badge = new QLabel(tr("● Connected"), this);
        badge->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(Style::dotGreen));
        action = badge;
    } else {
        const bool busy = tunnel.state != State::Disconnected;
        auto *button = new QPushButton(tunnel.state == State::Connecting ? tr("Connecting…")
                                       : busy                            ? tr("Disconnecting…")
                                                                         : tr("Connect"), this);
        button->setEnabled(!busy);
        button->setStyleSheet(Style::connectButton);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this] { emit connectClicked(m_uuid); });
        action = button;
    }

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->addLayout(text, 1);
    layout->addWidget(action);
}
