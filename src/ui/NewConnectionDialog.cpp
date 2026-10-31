#include "ui/NewConnectionDialog.h"

#include "core/ConfParser.h"
#include "core/KeyGen.h"
#include "core/ManualConfig.h"
#include "ui/Style.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QLabel *hint(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setEnabled(false); // muted
    return l;
}

}

NewConnectionDialog::NewConnectionDialog(const QStringList &takenNames, QWidget *parent)
    : QDialog(parent)
    , m_taken(takenNames)
    , m_name(new QLineEdit(this))
    , m_tabs(new QTabWidget(this))
    , m_error(new QLabel(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setWindowTitle(tr("New connection"));
    setMinimumWidth(520);

    m_name->setPlaceholderText(tr("For example: Home, Work, Spain"));
    m_name->setMaxLength(64);

    m_tabs->addTab(buildImportTab(), tr("Import a file"));
    m_tabs->addTab(buildManualTab(), tr("Enter details yourself"));

    m_error->setWordWrap(true);
    m_error->setStyleSheet(QStringLiteral("color: %1;").arg(Style::dotRed));

    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Create"));
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *nameForm = new QFormLayout;
    nameForm->addRow(tr("Connection name"), m_name);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(nameForm);
    layout->addWidget(m_tabs, 1);
    layout->addWidget(m_error);
    layout->addWidget(m_buttons);

    connect(m_name, &QLineEdit::textChanged, this, &NewConnectionDialog::validate);
    connect(m_tabs, &QTabWidget::currentChanged, this, &NewConnectionDialog::validate);
    m_ready = true;
    validate();
}

QString NewConnectionDialog::name() const
{
    return m_name->text().trimmed();
}

QWidget *NewConnectionDialog::buildImportTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel(tr("Your VPN provider gives you a file ending in <b>.conf</b>. Choose it here."), page));

    auto *choose = new QPushButton(QIcon::fromTheme("document-open"), tr("Choose file…"), page);
    connect(choose, &QPushButton::clicked, this, &NewConnectionDialog::chooseFile);
    layout->addWidget(choose, 0, Qt::AlignLeft);

    m_fileLabel = new QLabel(tr("No file chosen"), page);
    m_fileLabel->setWordWrap(true);
    m_fileLabel->setEnabled(false);
    layout->addWidget(m_fileLabel);
    layout->addStretch(1);
    return page;
}

QWidget *NewConnectionDialog::buildManualTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    const auto field = [&](const QString &placeholder) {
        auto *e = new QLineEdit(page);
        e->setPlaceholderText(placeholder);
        connect(e, &QLineEdit::textChanged, this, &NewConnectionDialog::validate);
        return e;
    };

    m_privateKey = field(tr("Paste it, or press Generate"));
    m_privateKey->setEchoMode(QLineEdit::Password);
    auto *reveal = m_privateKey->addAction(QIcon::fromTheme("view-visible"), QLineEdit::TrailingPosition);
    reveal->setCheckable(true);
    reveal->setToolTip(tr("Show or hide the key"));
    connect(reveal, &QAction::toggled, this, [this](bool on) {
        m_privateKey->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });

    auto *generate = new QPushButton(tr("Generate"), page);
    generate->setToolTip(tr("Make a new key for this device"));
    connect(generate, &QPushButton::clicked, this, [this] {
        m_privateKey->setText(KeyGen::generate().privateKey);
    });
    auto *keyRow = new QHBoxLayout;
    keyRow->addWidget(m_privateKey, 1);
    keyRow->addWidget(generate);

    m_publicKey = new QLineEdit(page);
    m_publicKey->setReadOnly(true);
    m_publicKey->setPlaceholderText(tr("Appears once you have a private key"));

    m_address = field(tr("10.0.0.2/24"));
    m_dns = field(tr("Optional, for example 1.1.1.1"));

    auto *device = new QFormLayout;
    device->addRow(tr("Private key"), keyRow);
    device->addRow(tr("Your public key"), m_publicKey);
    device->addRow(QString(), hint(tr("Give your public key to your VPN provider or server owner."), page));
    device->addRow(tr("Your address"), m_address);
    device->addRow(tr("DNS"), m_dns);

    m_serverKey = field(tr("The server's public key"));
    m_endpoint = field(tr("vpn.example.com:51820"));
    m_allowedIps = field(QString());
    m_allowedIps->setText(QStringLiteral("0.0.0.0/0, ::/0"));
    m_allowedIps->setToolTip(tr("0.0.0.0/0, ::/0 sends all your internet traffic through the VPN"));

    auto *server = new QFormLayout;
    server->addRow(tr("Server public key"), m_serverKey);
    server->addRow(tr("Server address"), m_endpoint);
    server->addRow(tr("Traffic to send"), m_allowedIps);

    m_presharedKey = field(tr("Optional"));
    m_presharedKey->setEchoMode(QLineEdit::Password);
    m_keepalive = field(tr("Optional, for example 25"));
    m_keepalive->setText(QStringLiteral("25"));
    m_mtu = field(tr("Optional"));

    auto *advanced = new QGroupBox(tr("Advanced"), page);
    advanced->setCheckable(true);
    advanced->setChecked(false);
    auto *advancedForm = new QFormLayout(advanced);
    advancedForm->addRow(tr("Preshared key"), m_presharedKey);
    advancedForm->addRow(tr("Keepalive (seconds)"), m_keepalive);
    advancedForm->addRow(tr("MTU"), m_mtu);
    // collapsed by default: hide the rows, keep the title and checkbox
    const auto showAdvanced = [advancedForm](bool on) {
        for (int i = 0; i < advancedForm->rowCount(); ++i) {
            for (const auto role : {QFormLayout::LabelRole, QFormLayout::FieldRole}) {
                if (QLayoutItem *item = advancedForm->itemAt(i, role); item && item->widget())
                    item->widget()->setVisible(on);
            }
        }
    };
    showAdvanced(false);
    connect(advanced, &QGroupBox::toggled, this, showAdvanced);

    layout->addLayout(device);
    layout->addSpacing(6);
    layout->addLayout(server);
    layout->addWidget(advanced);
    layout->addStretch(1);
    return page;
}

void NewConnectionDialog::chooseFile()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Choose a WireGuard file"), {},
                                                      tr("WireGuard files (*.conf);;All files (*)"));
    if (file.isEmpty())
        return;

    m_importOk = false;
    m_importError.clear();
    m_importWarnings.clear();
    m_fileLabel->setText(QFileInfo(file).fileName());
    m_fileLabel->setEnabled(true);

    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_importError = tr("Could not read that file: %1").arg(f.errorString());
    } else {
        const ParseResult parsed = ConfParser::parse(QString::fromUtf8(f.readAll()));
        if (parsed.ok()) {
            m_importOk = true;
            m_imported = parsed.config;
            m_importWarnings = parsed.warnings;
        } else {
            m_importError = tr("That file doesn't look like a valid WireGuard config: %1").arg(parsed.errors.first());
        }
    }

    if (m_importOk && m_name->text().trimmed().isEmpty())
        m_name->setText(QFileInfo(file).completeBaseName());
    validate();
}

void NewConnectionDialog::validate()
{
    if (!m_ready)
        return;

    QString problem;
    m_configValid = false;

    const bool importTab = m_tabs->currentIndex() == 0;

    // keep the derived public key in step with what is typed
    const QString pub = KeyGen::publicKeyFor(m_privateKey->text().trimmed());
    m_publicKey->setText(pub);

    if (name().isEmpty()) {
        problem = tr("Give the connection a name.");
    } else if (std::any_of(m_taken.cbegin(), m_taken.cend(), [this](const QString &n) {
                   return n.compare(name(), Qt::CaseInsensitive) == 0;
               })) {
        problem = tr("You already have a connection called \"%1\".").arg(name());
    } else if (importTab) {
        if (!m_importError.isEmpty())
            problem = m_importError;
        else if (!m_importOk)
            problem = tr("Choose the file from your VPN provider.");
        else
            m_config = m_imported;
        m_configValid = problem.isEmpty();
    } else {
        ManualInput in;
        in.privateKey = m_privateKey->text();
        in.address = m_address->text();
        in.dns = m_dns->text();
        in.serverPublicKey = m_serverKey->text();
        in.endpoint = m_endpoint->text();
        in.allowedIps = m_allowedIps->text();
        in.presharedKey = m_presharedKey->text();
        in.keepalive = m_keepalive->text();
        in.mtu = m_mtu->text();

        const ParseResult r = ManualConfig::build(in);
        if (!r.ok())
            problem = r.errors.first();
        else
            m_config = r.config;
        m_configValid = problem.isEmpty();
    }

    QString note = problem;
    if (problem.isEmpty() && importTab && !m_importWarnings.isEmpty())
        note = tr("Some settings in the file are not supported and will be skipped.");
    m_error->setText(note);
    m_error->setVisible(!note.isEmpty());
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(m_configValid);
}
