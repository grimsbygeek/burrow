#include "ui/PreferencesDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(Preferences &prefs, QWidget *parent)
    : QDialog(parent)
    , m_prefs(prefs)
    , m_original(prefs.theme())
    , m_theme(new QComboBox(this))
    , m_layout(new QComboBox(this))
    , m_tray(new QCheckBox(tr("Keep running in the system tray when the window is closed"), this))
{
    setWindowTitle(tr("Preferences"));

    m_theme->addItem(tr("Match system"), int(Preferences::Theme::System));
    m_theme->addItem(tr("Light"), int(Preferences::Theme::Light));
    m_theme->addItem(tr("Dark"), int(Preferences::Theme::Dark));
    m_theme->setCurrentIndex(m_theme->findData(int(m_original)));

    m_layout->addItem(tr("Simple"), true);
    m_layout->addItem(tr("Detailed"), false);
    m_layout->setCurrentIndex(prefs.simpleLayout() ? 0 : 1);
    m_layout->setToolTip(tr("Simple hides technical details until you ask for them"));

    m_tray->setChecked(prefs.closeToTray());
    m_tray->setToolTip(tr("Only has an effect on desktops that show a system tray"));

    // Preview straight away; Cancel puts the old theme back.
    connect(m_theme, &QComboBox::currentIndexChanged, this, [this] {
        Theme::apply(Preferences::Theme(m_theme->currentData().toInt()));
    });

    auto *form = new QFormLayout;
    form->addRow(tr("Theme"), m_theme);
    form->addRow(tr("Layout"), m_layout);
    form->addRow(QString(), m_tray);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        m_prefs.setTheme(Preferences::Theme(m_theme->currentData().toInt()));
        m_prefs.setSimpleLayout(m_layout->currentData().toBool());
        m_prefs.setCloseToTray(m_tray->isChecked());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::rejected, this, [this] { Theme::apply(m_original); });

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}
