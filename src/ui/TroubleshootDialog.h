#pragma once

#include "core/Diagnostics.h"

#include <QDialog>
#include <functional>

class QLabel;
class QVBoxLayout;

// Runs the checks and shows the results in plain words, with what to try for anything that is wrong.
// The caller supplies a function that gathers the facts, so "Run checks again" always sees fresh state.
class TroubleshootDialog : public QDialog {
    Q_OBJECT
public:
    using InputProvider = std::function<Diagnostics::Input()>;

    // `setupMode` is the first-run wording ("Let's check your system") instead of "Troubleshoot".
    TroubleshootDialog(const QString &title, InputProvider provider, bool setupMode, QWidget *parent = nullptr);

    QList<Diagnostics::Finding> findings() const { return m_findings; }

private:
    void runChecks();
    void copyReport();

    InputProvider m_provider;
    bool m_setupMode;
    Diagnostics::Input m_input;
    QList<Diagnostics::Finding> m_findings;

    QLabel *m_summary;
    QLabel *m_copied;
    QWidget *m_results;
};
