#include "ui/ExportDialog.h"

#include "core/ConfWriter.h"
#include "core/QrCode.h"
#include "ui/Style.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

ExportDialog::ExportDialog(const QString &connectionName, const QString &confText, QWidget *parent)
    : QDialog(parent)
    , m_name(connectionName)
    , m_text(confText)
    , m_qr(QrCode::render(confText))
    , m_qrButton(new QPushButton(QIcon::fromTheme("view-barcode-qr", QIcon::fromTheme("view-visible")), tr("Show QR code"), this))
    , m_qrLabel(new QLabel(this))
    , m_qrHint(new QLabel(tr("In the WireGuard app on your phone, choose “Add a tunnel”, then “Scan from QR code”."), this))
    , m_status(new QLabel(this))
{
    setWindowTitle(tr("Export “%1”").arg(connectionName));
    setMinimumWidth(420);

    auto *warning = new QLabel(tr("<b>Keep this private.</b> The exported connection contains your private key. "
                                  "Anyone who gets it can use your VPN connection."), this);
    warning->setWordWrap(true);
    warning->setStyleSheet(QStringLiteral("color: %1;").arg(Style::dotAmber));

    auto *save = new QPushButton(QIcon::fromTheme("document-save"), tr("Save as file…"), this);
    connect(save, &QPushButton::clicked, this, &ExportDialog::saveToFile);
    connect(m_qrButton, &QPushButton::clicked, this, &ExportDialog::toggleQr);
    if (m_qr.isNull()) {
        m_qrButton->setEnabled(false);
        m_qrButton->setToolTip(tr("This connection is too large to fit in a QR code"));
    }

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(save);
    buttons->addWidget(m_qrButton);
    buttons->addStretch(1);

    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->hide();
    m_qrHint->setWordWrap(true);
    m_qrHint->setAlignment(Qt::AlignCenter);
    m_qrHint->hide();
    m_status->setWordWrap(true);
    m_status->hide();

    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(warning);
    layout->addLayout(buttons);
    layout->addWidget(m_qrLabel);
    layout->addWidget(m_qrHint);
    layout->addWidget(m_status);
    layout->addWidget(close);
}

void ExportDialog::saveToFile()
{
    QString suggested = m_name;
    suggested.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"));

    const QString path = QFileDialog::getSaveFileName(this, tr("Save WireGuard file"),
                                                      QDir::home().filePath(suggested + QStringLiteral(".conf")),
                                                      tr("WireGuard files (*.conf)"));
    if (path.isEmpty())
        return;

    const QString error = ConfWriter::saveFile(path, m_text);
    m_status->setText(error.isEmpty() ? tr("Saved to %1. Only you can read this file.").arg(path)
                                      : tr("Could not save the file: %1").arg(error));
    m_status->setStyleSheet(error.isEmpty() ? QString() : QStringLiteral("color: %1;").arg(Style::dotRed));
    m_status->show();
}

void ExportDialog::toggleQr()
{
    const bool show = !m_qrLabel->isVisible();
    if (show)
        m_qrLabel->setPixmap(QPixmap::fromImage(m_qr)); // always black on white, even in the dark theme
    m_qrLabel->setVisible(show);
    m_qrHint->setVisible(show);
    m_qrButton->setText(show ? tr("Hide QR code") : tr("Show QR code"));
    adjustSize();
}
