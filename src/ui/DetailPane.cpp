#include "ui/DetailPane.h"

#include "core/NetStats.h"
#include "core/TunnelDetails.h"
#include "ui/MapBackground.h"
#include "ui/Style.h"

#include <QFormLayout>
#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QLabel *valueLabel(const QString &text)
{
    auto *l = new QLabel(text.isEmpty() ? QStringLiteral("—") : text);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setWordWrap(true);
    return l;
}

QLabel *heading(const QString &text)
{
    auto *l = new QLabel(text);
    QFont f = l->font();
    f.setBold(true);
    l->setFont(f);
    return l;
}

}

DetailPane::DetailPane(QWidget *parent)
    : QWidget(parent)
    , m_stack(new QStackedWidget(this))
    , m_status(new QLabel(this))
    , m_title(new QLabel(this))
    , m_editButton(new QToolButton(this))
    , m_exportButton(new QToolButton(this))
    , m_deleteButton(new QToolButton(this))
    , m_nameEdit(new QLineEdit(this))
    , m_subtitle(new QLabel(this))
    , m_body(new QWidget(this))
    , m_disconnect(new QPushButton(tr("Disconnect"), this))
    , m_action(new QPushButton(this))
    , m_toggleDetails(new QToolButton(this))
    , m_sentValue(new QLabel(this))
    , m_receivedValue(new QLabel(this))
    , m_summary(new QLabel(this))
    , m_failureBar(new QWidget(this))
    , m_failureText(new QLabel(this))
    , m_emptyText(new QLabel(this))
    , m_emptyImport(new QPushButton(tr("Add a connection"), this))
{
    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.4);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_subtitle->setEnabled(false);
    m_status->setAlignment(Qt::AlignCenter);
    m_status->setTextFormat(Qt::RichText);
    QFont statusFont = m_status->font();
    statusFont.setPointSizeF(statusFont.pointSizeF() * 1.15);
    m_status->setFont(statusFont);
    m_subtitle->setAlignment(Qt::AlignCenter);

    m_editButton->setIcon(QIcon::fromTheme("document-edit"));
    m_editButton->setToolTip(tr("Rename (only changes the name shown in this app)"));
    m_editButton->setAutoRaise(true);
    m_exportButton->setIcon(QIcon::fromTheme("document-export"));
    m_exportButton->setToolTip(tr("Export or share this connection (file or QR code)"));
    m_exportButton->setAutoRaise(true);
    connect(m_exportButton, &QToolButton::clicked, this, [this] { emit exportRequested(m_uuid); });
    m_deleteButton->setIcon(QIcon::fromTheme("edit-delete"));
    m_deleteButton->setToolTip(tr("Delete this connection"));
    m_deleteButton->setAutoRaise(true);
    connect(m_deleteButton, &QToolButton::clicked, this, [this] { emit deleteRequested(m_uuid); });
    m_nameEdit->setFont(titleFont);
    m_nameEdit->setMaxLength(64);
    m_nameEdit->setAlignment(Qt::AlignCenter);
    m_nameEdit->setMinimumWidth(260);
    m_nameEdit->setToolTip(tr("Enter to save, Esc to cancel"));
    m_nameEdit->hide();
    auto *cancel = new QAction(m_nameEdit);
    cancel->setShortcut(Qt::Key_Escape);
    cancel->setShortcutContext(Qt::WidgetShortcut);
    m_nameEdit->addAction(cancel);
    connect(cancel, &QAction::triggered, this, [this] { finishRename(false); });
    connect(m_nameEdit, &QLineEdit::returnPressed, this, [this] { finishRename(true); });
    connect(m_editButton, &QToolButton::clicked, this, &DetailPane::startRename);
    m_disconnect->setStyleSheet(Style::disconnectButton);
    m_disconnect->setCursor(Qt::PointingHandCursor);

    QFont big = m_action->font();
    big.setPointSizeF(big.pointSizeF() * 1.3);
    big.setBold(true);
    m_action->setFont(big);
    m_action->setMinimumHeight(52);
    m_action->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_action->setCursor(Qt::PointingHandCursor);

    m_toggleDetails->setAutoRaise(true);
    m_toggleDetails->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toggleDetails->setCursor(Qt::PointingHandCursor);
    connect(m_toggleDetails, &QToolButton::clicked, this, [this] {
        m_detailsOpen = !m_detailsOpen;
        applyMode();
    });

    m_emptyText->setAlignment(Qt::AlignCenter);
    m_emptyText->setWordWrap(true);
    m_emptyImport->setStyleSheet(Style::connectButton);
    m_emptyImport->setMinimumSize(200, 48);
    m_emptyImport->setCursor(Qt::PointingHandCursor);
    connect(m_emptyImport, &QPushButton::clicked, this, &DetailPane::importRequested);

    auto *empty = new QWidget(this);
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->addStretch(1);
    emptyLayout->addWidget(m_emptyText);
    emptyLayout->addSpacing(12);
    emptyLayout->addWidget(m_emptyImport, 0, Qt::AlignHCenter);
    emptyLayout->addStretch(1);

    // Thin connection-details strip: sent and received side by side, server and address underneath.
    const auto statCard = [this, big](const QString &caption, QLabel *value) {
        auto *cap = new QLabel(caption, this);
        cap->setAlignment(Qt::AlignCenter);
        cap->setEnabled(false);
        QFont valueFont = big;
        valueFont.setPointSizeF(valueFont.pointSizeF() * 0.85);
        value->setFont(valueFont);
        value->setAlignment(Qt::AlignCenter);
        auto *card = new QVBoxLayout;
        card->setSpacing(0);
        card->addWidget(cap);
        card->addWidget(value);
        return card;
    };
    m_summary->setAlignment(Qt::AlignCenter);
    m_summary->setWordWrap(true);
    m_summary->setEnabled(false);

    auto *stats = new QWidget(this);
    auto *statsLayout = new QVBoxLayout(stats);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setSpacing(4);
    auto *cards = new QHBoxLayout;
    cards->addLayout(statCard(tr("↑ Sent"), m_sentValue), 1);
    cards->addLayout(statCard(tr("↓ Received"), m_receivedValue), 1);
    statsLayout->addLayout(cards);
    statsLayout->addWidget(m_summary);
    clearTraffic();

    // shown when the last attempt failed: says why, and offers the troubleshooting checks
    m_failureText->setWordWrap(true);
    m_failureText->setTextFormat(Qt::RichText);
    auto *troubleshoot = new QPushButton(QIcon::fromTheme("help-contents"), tr("Troubleshoot…"), this);
    connect(troubleshoot, &QPushButton::clicked, this, [this] { emit troubleshootRequested(m_uuid); });
    auto *failureLayout = new QHBoxLayout(m_failureBar);
    failureLayout->setContentsMargins(10, 8, 10, 8);
    failureLayout->addWidget(m_failureText, 1);
    failureLayout->addWidget(troubleshoot);
    m_failureBar->setStyleSheet(QStringLiteral("QWidget#failureBar { border: 1px solid %1; border-radius: 6px; }").arg(Style::dotRed));
    m_failureBar->setObjectName("failureBar");
    m_failureBar->hide();

    auto *page = new MapBackground(this);
    auto *pageLayout = new QVBoxLayout(page);
    auto *titleRow = new QHBoxLayout;
    titleRow->addStretch(1);
    titleRow->addWidget(m_title);
    titleRow->addWidget(m_nameEdit);
    titleRow->addWidget(m_editButton);
    titleRow->addWidget(m_exportButton);
    titleRow->addWidget(m_deleteButton);
    titleRow->addStretch(1);
    pageLayout->addLayout(titleRow);
    pageLayout->addWidget(m_status);
    pageLayout->addWidget(m_failureBar);
    pageLayout->addWidget(m_subtitle);
    pageLayout->addSpacing(8);
    pageLayout->addWidget(stats);
    pageLayout->addWidget(m_toggleDetails, 0, Qt::AlignHCenter);
    pageLayout->addWidget(m_body);
    pageLayout->addStretch(1);
    pageLayout->addWidget(m_action); // wide, at the bottom
    pageLayout->addWidget(m_disconnect, 0, Qt::AlignRight);

    m_stack->addWidget(empty); // index 0
    m_stack->addWidget(page);  // index 1

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);

    connect(m_disconnect, &QPushButton::clicked, this, [this] { emit disconnectClicked(m_uuid); });
    connect(m_action, &QPushButton::clicked, this, [this] {
        if (m_state == Tunnel::State::Disconnected)
            emit connectClicked(m_uuid);
        else
            emit disconnectClicked(m_uuid);
    });
    applyMode();
    clear();
}

void DetailPane::startRename()
{
    m_nameEdit->setText(m_currentName);
    m_title->hide();
    m_editButton->hide();
    m_exportButton->hide();
    m_deleteButton->hide();
    m_nameEdit->show();
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void DetailPane::finishRename(bool accept)
{
    if (!m_nameEdit->isVisible())
        return;
    // hide first: removing focus would otherwise re-enter through editingFinished-style paths
    m_nameEdit->hide();
    m_title->show();
    m_editButton->show();
    m_exportButton->show();
    m_deleteButton->show();
    if (accept && !m_uuid.isEmpty())
        emit renameRequested(m_uuid, m_nameEdit->text());
}

void DetailPane::setTraffic(quint64 sentBytes, quint64 receivedBytes)
{
    m_sentValue->setText(NetStats::formatBytes(sentBytes));
    m_receivedValue->setText(NetStats::formatBytes(receivedBytes));
}

void DetailPane::clearTraffic()
{
    m_sentValue->setText(QStringLiteral("—"));
    m_receivedValue->setText(QStringLiteral("—"));
}

void DetailPane::setSimple(bool simple)
{
    m_simple = simple;
    applyMode();
}

void DetailPane::applyMode()
{
    m_subtitle->setVisible(!m_simple);
    m_disconnect->setVisible(!m_simple);
    m_action->setVisible(m_simple);
    m_toggleDetails->setVisible(m_simple);
    m_body->setVisible(!m_simple || m_detailsOpen);

    m_toggleDetails->setText(m_detailsOpen ? tr("Hide details") : tr("Show details"));
    m_toggleDetails->setArrowType(m_detailsOpen ? Qt::DownArrow : Qt::RightArrow);
}

void DetailPane::clear(bool haveTunnels)
{
    finishRename(false);
    m_uuid.clear();
    m_emptyText->setText(haveTunnels
        ? tr("Pick a connection on the left")
        : tr("<h2>Welcome</h2><p>You don't have any VPN connections yet.<br>"
             "Add one by importing the file from your VPN provider, or by entering the details yourself.</p>"));
    m_emptyImport->setVisible(!haveTunnels);
    m_stack->setCurrentIndex(0);
}

void DetailPane::showTunnel(const Tunnel &tunnel)
{
    if (tunnel.uuid != m_uuid)
        finishRename(false); // a state refresh of the same tunnel must not cancel an edit in progress
    m_uuid = tunnel.uuid;
    m_currentName = tunnel.label();
    m_title->setText(m_currentName);
    const auto dotted = [](const QString &color, const QString &text) {
        return QStringLiteral("<span style=\"color:%1;\">●</span>&nbsp; %2").arg(color, text);
    };
    switch (tunnel.state) {
    case Tunnel::State::Connected: m_status->setText(dotted(Style::dotGreen, tr("Connected"))); break;
    case Tunnel::State::Connecting: m_status->setText(dotted(Style::dotAmber, tr("Connecting…"))); break;
    case Tunnel::State::Disconnecting: m_status->setText(dotted(Style::dotAmber, tr("Disconnecting…"))); break;
    case Tunnel::State::Disconnected: m_status->setText(dotted(Style::dotRed, tr("Not connected"))); break;
    }
    m_subtitle->setText(tunnel.interfaceName);

    m_failureText->setText(tr("<b>Couldn't connect.</b> %1").arg(tunnel.failure.toHtmlEscaped()));
    m_failureBar->setVisible(!tunnel.failure.isEmpty() && tunnel.state == Tunnel::State::Disconnected);

    m_state = tunnel.state;
    const bool connected = tunnel.state == Tunnel::State::Connected;
    const bool busy = tunnel.state == Tunnel::State::Connecting || tunnel.state == Tunnel::State::Disconnecting;
    m_action->setText(connected                                    ? tr("Disconnect")
                      : tunnel.state == Tunnel::State::Connecting    ? tr("Connecting…")
                      : tunnel.state == Tunnel::State::Disconnecting ? tr("Disconnecting…")
                                                                     : tr("Connect"));
    m_action->setStyleSheet(connected ? Style::disconnectButton : Style::connectButton);
    m_action->setEnabled(!busy);
    m_disconnect->setEnabled(tunnel.state == Tunnel::State::Connected || tunnel.state == Tunnel::State::Connecting);

    // Rebuild the body from scratch; tunnels are few and this runs on selection only.
    delete m_body->layout();
    qDeleteAll(m_body->children());
    auto *layout = new QVBoxLayout(m_body);
    layout->setContentsMargins(0, 0, 0, 0);

    const TunnelDetails d = TunnelDetailsReader::fromSettings(tunnel.settings);

    QStringList summary;
    if (!d.peers.isEmpty() && !d.peers.first().endpoint.isEmpty())
        summary << tr("Server %1").arg(d.peers.first().endpoint);
    if (!d.addresses.isEmpty())
        summary << tr("Your address %1").arg(d.addresses.first());
    m_summary->setText(summary.join(QStringLiteral("   ·   ")));
    m_summary->setVisible(!summary.isEmpty());

    layout->addWidget(heading(tr("Interface")));
    auto *iface = new QFormLayout;
    iface->addRow(tr("Address"), valueLabel(d.addresses.join(", ")));
    iface->addRow(tr("DNS"), valueLabel(d.dns.join(", ")));
    iface->addRow(tr("Listen port"), valueLabel(d.listenPort ? QString::number(d.listenPort) : QString()));
    iface->addRow(tr("MTU"), valueLabel(d.mtu ? QString::number(d.mtu) : QString()));
    iface->addRow(tr("Autoconnect"), valueLabel(tunnel.autoconnect ? tr("Yes") : tr("No")));
    layout->addLayout(iface);

    for (int i = 0; i < d.peers.size(); ++i) {
        const PeerDetails &p = d.peers[i];
        auto *line = new QFrame;
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        layout->addSpacing(6);
        layout->addWidget(line);
        layout->addWidget(heading(d.peers.size() > 1 ? tr("Peer %1").arg(i + 1) : tr("Peer")));

        auto *form = new QFormLayout;
        form->addRow(tr("Public key"), valueLabel(p.publicKey));
        form->addRow(tr("Endpoint"), valueLabel(p.endpoint));
        form->addRow(tr("Allowed IPs"), valueLabel(p.allowedIps.join(", ")));
        form->addRow(tr("Keepalive"), valueLabel(p.persistentKeepalive ? tr("%1 s").arg(p.persistentKeepalive) : QString()));
        layout->addLayout(form);
    }
    layout->addStretch(1);

    m_stack->setCurrentIndex(1);
}
