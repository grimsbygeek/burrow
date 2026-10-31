#include "ui/MainWindow.h"

#include "core/ConfWriter.h"
#include "core/Diagnostics.h"
#include "core/NetStats.h"
#include "core/NmSettingsMapper.h"
#include "nm/NmClient.h"
#include "ui/DetailPane.h"
#include "ui/Links.h"
#include "ui/ExportDialog.h"
#include "ui/NewConnectionDialog.h"
#include "ui/PreferencesDialog.h"
#include "ui/TrayController.h"
#include "ui/TroubleshootDialog.h"
#include "ui/TunnelRow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QSysInfo>
#include <algorithm>
#include <QDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStatusBar>
#include <QUrl>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_nm(new NmClient(QDBusConnection::systemBus(), this))
    , m_list(new QListWidget(this))
    , m_detail(new DetailPane(this))
{
    setWindowTitle(tr("Burrow"));
    resize(840, 520);

    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *left = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_list, 1);

    // Voluntary support link: quiet on purpose, and it only opens the browser. Flathub's monetisation
    // policy excludes donations, so this is allowed.
    auto *coffee = new QPushButton(tr("☕  Buy me a coffee"), this);
    coffee->setFlat(true);
    coffee->setCursor(Qt::PointingHandCursor);
    coffee->setToolTip(tr("Support the developer (opens your web browser)"));
    connect(coffee, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(Links::donate)); });
    leftLayout->addWidget(coffee);

    auto *divider = new QFrame(this);
    divider->setFrameShape(QFrame::VLine);
    divider->setFrameShadow(QFrame::Sunken);

    auto *panes = new QHBoxLayout;
    panes->addWidget(left, 1);     // 1/3
    panes->addWidget(divider);
    panes->addWidget(m_detail, 2); // 2/3

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->addWidget(buildHeader());
    root->addLayout(panes, 1);
    setCentralWidget(central);

    connect(m_list, &QListWidget::currentRowChanged, this, &MainWindow::showSelected);
    m_detail->setSimple(m_prefs.simpleLayout());
    connect(m_detail, &DetailPane::troubleshootRequested, this, &MainWindow::troubleshoot);
    connect(m_detail, &DetailPane::exportRequested, this, &MainWindow::exportTunnel);
    connect(m_detail, &DetailPane::deleteRequested, this, &MainWindow::deleteTunnel);
    connect(m_detail, &DetailPane::importRequested, this, &MainWindow::newConnection);
    connect(m_detail, &DetailPane::connectClicked, m_nm, &NmClient::connectTunnel);
    connect(m_detail, &DetailPane::renameRequested, this, &MainWindow::renameTunnel);
    connect(m_detail, &DetailPane::disconnectClicked, m_nm, &NmClient::disconnectTunnel);
    connect(m_nm, &NmClient::tunnelsChanged, this, &MainWindow::rebuildList);
    connect(m_nm, &NmClient::errorOccurred, this, [this](const QString &msg) {
        statusBar()->showMessage(msg);
    });

    auto *trafficTimer = new QTimer(this);
    connect(trafficTimer, &QTimer::timeout, this, &MainWindow::updateTraffic);
    trafficTimer->start(1000);

    if (TrayController::isAvailable()) {
        m_tray = new TrayController(this);
        connect(m_tray, &TrayController::toggleWindowRequested, this, &MainWindow::toggleWindow);
        connect(m_tray, &TrayController::openRequested, this, &MainWindow::showWindow);
        connect(m_tray, &TrayController::connectRequested, m_nm, &NmClient::connectTunnel);
        connect(m_tray, &TrayController::disconnectRequested, m_nm, &NmClient::disconnectTunnel);
        connect(m_tray, &TrayController::quitRequested, this, &MainWindow::quit);
        m_tray->show();
    }

    m_nm->refresh();
    QTimer::singleShot(0, this, &MainWindow::firstRunCheck); // after the window is shown
}

void MainWindow::quit()
{
    m_quitting = true;
    qApp->quit();
}

void MainWindow::showWindow()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::toggleWindow()
{
    if (isVisible() && !isMinimized())
        hide();
    else
        showWindow();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // a desktop logout also closes windows: never hold that up
    if (m_tray && m_prefs.closeToTray() && !m_quitting && !qApp->isSavingSession()) {
        hide();
        event->ignore();
        if (!m_trayHintShown) {
            m_trayHintShown = true;
            m_tray->notify(tr("Burrow"), tr("Still running in the system tray. Right-click the icon to quit."));
        }
        return;
    }
    event->accept();
    quit();
}

QWidget *MainWindow::buildHeader()
{
    auto *logo = new QLabel(this);
    logo->setPixmap(QIcon(":/icons/wgtray-64.png").pixmap(36));

    auto *title = new QLabel(tr("Burrow"), this);
    QFont f = title->font();
    f.setPointSizeF(f.pointSizeF() * 1.5);
    f.setBold(true);
    title->setFont(f);

    auto *menu = new QMenu(this);
    menu->addAction(QIcon::fromTheme("preferences-system"), tr("Preferences…"), this, [this] {
        if (PreferencesDialog(m_prefs, this).exec() == QDialog::Accepted) {
            m_detail->setSimple(m_prefs.simpleLayout());
            rebuildList();
        }
    });
    menu->addAction(QIcon::fromTheme("help-contents"), tr("Troubleshoot…"), this, [this] { troubleshoot(m_selectedUuid); });
    menu->addSeparator();
    menu->addAction(tr("About"), this, [this] {
        QMessageBox box(this);
        box.setWindowTitle(tr("About"));
        box.setIconPixmap(QIcon(":/icons/wgtray-64.png").pixmap(64));
        box.setTextFormat(Qt::RichText);
        box.setText(tr("<h3>Burrow %1</h3>"
                       "<p>A simple way to import, create and switch WireGuard connections on Linux.</p>"
                       "<p>Licensed under GPL-3.0-or-later.</p>"
                       "<p>If it saves you time, you can <a href=\"%2\">buy me a coffee ☕</a>.</p>"
                       "<p><small>Independent project, not affiliated with or endorsed by the WireGuard project. "
                       "WireGuard is a registered trademark of Jason A. Donenfeld.</small></p>")
                        .arg(QApplication::applicationVersion(), Links::donate));
        box.setTextInteractionFlags(Qt::TextBrowserInteraction); // makes the link clickable
        box.exec();
    });

    auto *importButton = new QPushButton(QIcon::fromTheme("list-add"), tr("New connection"), this);
    importButton->setToolTip(tr("Import a file or enter the details yourself"));
    connect(importButton, &QPushButton::clicked, this, &MainWindow::newConnection);

    auto *cog = new QToolButton(this);
    cog->setIcon(QIcon::fromTheme("preferences-system", QIcon::fromTheme("configure")));
    cog->setToolTip(tr("Settings"));
    cog->setAutoRaise(true);
    cog->setPopupMode(QToolButton::InstantPopup);
    cog->setMenu(menu);

    auto *header = new QWidget(this);
    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(logo);
    layout->addWidget(title);
    layout->addStretch(1);
    layout->addWidget(importButton);
    layout->addWidget(cog);
    return header;
}

void MainWindow::rebuildList()
{
    trackStates();
    m_list->blockSignals(true);
    m_list->clear();
    int reselect = -1;
    const QList<Tunnel> tunnels = labelledTunnels();
    for (int i = 0; i < tunnels.size(); ++i) {
        auto *row = new TunnelRow(tunnels[i], m_prefs.simpleLayout(), m_list);
        connect(row, &TunnelRow::connectClicked, m_nm, &NmClient::connectTunnel);

        auto *item = new QListWidgetItem(m_list);
        item->setSizeHint(row->sizeHint());
        m_list->setItemWidget(item, row);
        if (tunnels[i].uuid == m_selectedUuid)
            reselect = i;
    }
    if (reselect < 0 && !tunnels.isEmpty())
        reselect = 0; // always have something showing, so the big button is right there
    m_list->setCurrentRow(reselect);
    m_list->blockSignals(false);

    showSelected();
    if (m_tray)
        m_tray->setTunnels(tunnels);
    if (!m_prefs.simpleLayout())
        statusBar()->showMessage(tr("%n tunnel(s)", nullptr, tunnels.size()));
}

void MainWindow::showSelected()
{
    const int row = m_list->currentRow();
    if (row < 0) {
        m_detail->clear(m_list->count() > 0);
        return;
    }
    const Tunnel tunnel = labelledTunnels().at(row);
    m_selectedUuid = tunnel.uuid;
    m_detail->showTunnel(tunnel);
    updateTraffic();
}

void MainWindow::updateTraffic()
{
    for (const Tunnel &t : m_nm->tunnels()) {
        if (t.uuid != m_selectedUuid)
            continue;
        const NetStats::Counters c = t.state == Tunnel::State::Connected ? NetStats::read(t.interfaceName) : NetStats::Counters{};
        if (c.ok)
            m_detail->setTraffic(c.txBytes, c.rxBytes);
        else
            m_detail->clearTraffic();
        return;
    }
    m_detail->clearTraffic();
}

QList<Tunnel> MainWindow::labelledTunnels() const
{
    QList<Tunnel> tunnels = m_nm->tunnels();
    for (Tunnel &t : tunnels)
        t.displayName = m_aliases.alias(t.uuid);
    return tunnels;
}

void MainWindow::renameTunnel(const QString &uuid, const QString &name)
{
    const QString trimmed = name.trimmed();
    for (const Tunnel &t : labelledTunnels()) {
        if (t.uuid != uuid && !trimmed.isEmpty() && t.label().compare(trimmed, Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, tr("Rename"), tr("Another tunnel is already called \"%1\".").arg(trimmed));
            return;
        }
    }
    m_aliases.setAlias(uuid, trimmed); // blank resets to the NetworkManager name
    rebuildList();
}

void MainWindow::trackStates()
{
    const QDateTime now = QDateTime::currentDateTime();
    QSet<QString> present;
    for (const Tunnel &t : m_nm->tunnels()) {
        present.insert(t.uuid);
        const auto seen = m_seen.constFind(t.uuid);
        if (seen != m_seen.constEnd() && seen->state == t.state)
            continue;

        QDateTime since = now;
        // already connected when the app started: NetworkManager remembers when it was last activated
        if (seen == m_seen.constEnd() && t.state == Tunnel::State::Connected) {
            const qint64 stamp = t.settings.value("connection").value("timestamp").toLongLong();
            if (stamp > 0 && stamp <= now.toSecsSinceEpoch())
                since = QDateTime::fromSecsSinceEpoch(stamp);
        }
        m_seen.insert(t.uuid, {t.state, since});
    }
    for (auto it = m_seen.begin(); it != m_seen.end();)
        it = present.contains(it.key()) ? std::next(it) : m_seen.erase(it);
}

Diagnostics::Input MainWindow::gatherDiagnostics(const QString &uuid) const
{
    Diagnostics::Input in;

    const NmSystemInfo sys = m_nm->systemInfo();
    in.nmRunning = sys.running;
    in.nmVersion = sys.version;
    in.nmState = sys.state;
    in.connectivity = sys.connectivity;
    in.mayModifyConnections = sys.mayModifyConnections;
    in.mayControlNetwork = sys.mayControlNetwork;
    in.activeDns = sys.dnsServers;

    in.kernelVersion = QSysInfo::kernelVersion();
    QFile modules(QStringLiteral("/proc/modules"));
    if (modules.open(QIODevice::ReadOnly))
        in.wireguardModuleLoaded = modules.readAll().contains("\nwireguard ") || QFile::exists(QStringLiteral("/sys/module/wireguard"));

    in.appVersion = QApplication::applicationVersion();
    in.osName = QSysInfo::prettyProductName();
    in.inFlatpak = QFile::exists(QStringLiteral("/.flatpak-info"));

    for (const Tunnel &t : m_nm->tunnels()) {
        if (t.uuid != uuid)
            continue;
        in.haveConnection = true;
        in.config = NmSettingsMapper::fromNmSettings(t.settings, {}); // no secrets needed for the checks
        in.state = t.state;
        in.lastFailure = t.failure;
        if (const auto seen = m_seen.constFind(uuid); seen != m_seen.constEnd())
            in.secondsInState = seen->since.secsTo(QDateTime::currentDateTime());
        if (t.state == Tunnel::State::Connected) {
            const NetStats::Counters c = NetStats::read(t.interfaceName);
            in.countersOk = c.ok;
            in.sentBytes = c.txBytes;
            in.receivedBytes = c.rxBytes;
        }
        break;
    }
    return in;
}

void MainWindow::troubleshoot(const QString &uuid)
{
    TroubleshootDialog dialog(tr("Troubleshoot"), [this, uuid] { return gatherDiagnostics(uuid); }, false, this);
    dialog.exec();
}

void MainWindow::firstRunCheck()
{
    if (m_startupCheckShown)
        return;
    m_startupCheckShown = true;

    const bool firstRun = !m_prefs.setupDone();
    const Diagnostics::Input in = gatherDiagnostics({}); // the system only
    const auto worst = Diagnostics::worstLevel(Diagnostics::run(in));

    using L = Diagnostics::Finding::Level;
    // The first time, show anything that isn't fine. After that, only interrupt for something that stops the
    // app working at all.
    const bool show = firstRun ? worst >= L::Warning : worst >= L::Problem;
    m_prefs.setSetupDone(true);
    if (!show)
        return;

    TroubleshootDialog dialog(tr("Let's check your system"), [this] { return gatherDiagnostics({}); }, true, this);
    dialog.exec();
}

void MainWindow::exportTunnel(const QString &uuid)
{
    const auto tunnels = labelledTunnels();
    const auto it = std::find_if(tunnels.cbegin(), tunnels.cend(), [&](const Tunnel &t) { return t.uuid == uuid; });
    if (it == tunnels.cend())
        return;

    // the private key is not part of the normal settings: ask NetworkManager for it
    NmSettings secrets;
    if (!m_nm->getSecrets(uuid, secrets))
        return; // errorOccurred() already put the reason in the status bar

    const WgConfig config = NmSettingsMapper::fromNmSettings(it->settings, secrets);
    if (config.privateKey.isEmpty()) {
        QMessageBox::warning(this, tr("Export"), tr("Could not read this connection's private key, so it can't be exported."));
        return;
    }
    ExportDialog(it->label(), ConfWriter::toWgQuick(config), this).exec();
}

void MainWindow::deleteTunnel(const QString &uuid)
{
    const auto tunnels = labelledTunnels();
    const auto it = std::find_if(tunnels.cbegin(), tunnels.cend(), [&](const Tunnel &t) { return t.uuid == uuid; });
    if (it == tunnels.cend())
        return;

    QMessageBox box(QMessageBox::Warning, tr("Delete connection"),
                    tr("Delete \"%1\"?").arg(it->label()), QMessageBox::NoButton, this);
    box.setInformativeText(it->state == Tunnel::State::Disconnected
        ? tr("This removes the connection and its key from this computer. You can't undo this.")
        : tr("It is connected right now, so it will be disconnected first. This removes the connection and its key from this computer. You can't undo this."));
    QPushButton *remove = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != remove)
        return;

    m_aliases.setAlias(uuid, QString()); // drop the app-local name too
    if (m_selectedUuid == uuid)
        m_selectedUuid.clear();
    m_nm->deleteTunnel(uuid);
}

void MainWindow::newConnection()
{
    QStringList takenNames, takenInterfaces;
    for (const Tunnel &t : labelledTunnels()) {
        takenNames << t.label() << t.id;
        takenInterfaces << t.interfaceName;
    }

    NewConnectionDialog dialog(takenNames, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const NmSettings settings = NmSettingsMapper::toNmSettings(
        dialog.config(), dialog.name(), NmSettingsMapper::uniqueInterfaceName(dialog.name(), takenInterfaces));

    m_selectedUuid = settings["connection"]["uuid"].toString(); // so the new connection is the one shown
    m_nm->addConnection(settings); // errorOccurred() reports a failure in the status bar
}
