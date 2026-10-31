#include "ui/TroubleshootDialog.h"

#include "ui/Style.h"

#include <QClipboard>
#include <algorithm>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

struct Look {
    const char *symbol;
    const QString &color;
};

Look lookFor(Diagnostics::Finding::Level level)
{
    using L = Diagnostics::Finding::Level;
    switch (level) {
    case L::Ok: return {"✓", Style::dotGreen};
    case L::Info: return {"ℹ", Style::dotMuted};
    case L::Warning: return {"⚠", Style::dotAmber};
    case L::Problem: return {"✕", Style::dotRed};
    }
    return {"•", Style::dotMuted};
}

}

TroubleshootDialog::TroubleshootDialog(const QString &title, InputProvider provider, bool setupMode, QWidget *parent)
    : QDialog(parent)
    , m_provider(std::move(provider))
    , m_setupMode(setupMode)
    , m_summary(new QLabel(this))
    , m_copied(new QLabel(this))
    , m_results(new QWidget(this))
{
    setWindowTitle(title);
    resize(560, 520);

    QFont big = m_summary->font();
    big.setPointSizeF(big.pointSizeF() * 1.25);
    big.setBold(true);
    m_summary->setFont(big);
    m_summary->setWordWrap(true);

    m_copied->setEnabled(false);
    m_copied->hide();

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(m_results);
    new QVBoxLayout(m_results);

    auto *again = new QPushButton(QIcon::fromTheme("view-refresh"), tr("Run checks again"), this);
    connect(again, &QPushButton::clicked, this, &TroubleshootDialog::runChecks);
    auto *copy = new QPushButton(QIcon::fromTheme("edit-copy"), tr("Copy report"), this);
    copy->setToolTip(tr("Copies a summary you can paste into a forum post or bug report. It contains no keys or full server addresses."));
    connect(copy, &QPushButton::clicked, this, &TroubleshootDialog::copyReport);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *row = new QHBoxLayout;
    row->addWidget(again);
    row->addWidget(copy);
    row->addWidget(m_copied, 1);
    row->addWidget(buttons);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_summary);
    layout->addWidget(scroll, 1);
    layout->addLayout(row);

    runChecks();
}

void TroubleshootDialog::runChecks()
{
    m_input = m_provider();
    m_findings = Diagnostics::run(m_input);
    // what needs attention first, the reassuring ticks last (stable: related checks stay in order)
    std::stable_sort(m_findings.begin(), m_findings.end(), [](const auto &a, const auto &b) { return a.level > b.level; });
    m_copied->hide();

    // rebuild the list
    auto *layout = static_cast<QVBoxLayout *>(m_results->layout());
    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    for (const Diagnostics::Finding &f : m_findings) {
        const Look look = lookFor(f.level);

        auto *symbol = new QLabel(QString::fromUtf8(look.symbol), m_results);
        symbol->setStyleSheet(QStringLiteral("color: %1; font-size: 18px; font-weight: bold;").arg(look.color));
        symbol->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        symbol->setFixedWidth(28);

        auto *title = new QLabel(f.title, m_results);
        QFont bold = title->font();
        bold.setBold(true);
        title->setFont(bold);
        title->setWordWrap(true);

        auto *text = new QVBoxLayout;
        text->setSpacing(2);
        text->addWidget(title);
        if (!f.detail.isEmpty()) {
            auto *detail = new QLabel(f.detail, m_results);
            detail->setWordWrap(true);
            text->addWidget(detail);
        }
        if (!f.fix.isEmpty()) {
            auto *fix = new QLabel(tr("<b>What to try:</b> %1").arg(f.fix.toHtmlEscaped().replace('\n', QStringLiteral("<br>"))), m_results);
            fix->setTextFormat(Qt::RichText);
            fix->setWordWrap(true);
            fix->setTextInteractionFlags(Qt::TextSelectableByMouse);
            text->addWidget(fix);
        }

        // one container widget per result, so clearing the list later deletes everything inside it
        auto *entry = new QWidget(m_results);
        auto *row = new QHBoxLayout(entry);
        row->setContentsMargins(0, 4, 0, 4);
        row->setAlignment(Qt::AlignTop);
        row->addWidget(symbol, 0, Qt::AlignTop);
        row->addLayout(text, 1);
        layout->addWidget(entry);
    }
    layout->addStretch(1);

    using L = Diagnostics::Finding::Level;
    const L worst = Diagnostics::worstLevel(m_findings);
    const int problems = int(std::count_if(m_findings.cbegin(), m_findings.cend(), [](const auto &f) { return f.level >= L::Warning; }));
    if (worst >= L::Warning) {
        m_summary->setText(problems == 1 ? tr("1 thing needs your attention") : tr("%1 things need your attention").arg(problems));
    } else {
        m_summary->setText(m_setupMode ? tr("Everything Burrow needs is here") : tr("No problems found"));
    }
}

void TroubleshootDialog::copyReport()
{
    QGuiApplication::clipboard()->setText(Diagnostics::report(m_input, m_findings));
    m_copied->setText(tr("Copied. Paste it where you're asking for help."));
    m_copied->show();
}
