#include "core/ConfParser.h"
#include "ui/TroubleshootDialog.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QtTest>

namespace {
Diagnostics::Input healthy()
{
    Diagnostics::Input in;
    in.nmRunning = true;
    in.nmVersion = "1.56.1";
    in.nmState = 70;
    in.connectivity = 4;
    in.kernelVersion = "6.11.2";
    in.appVersion = "0.1.0";
    in.osName = "Test OS";
    return in;
}

QStringList allText(QWidget *w)
{
    QStringList out;
    for (const QLabel *l : w->findChildren<QLabel *>())
        out << l->text();
    return out;
}

QPushButton *button(QWidget *w, const QString &text)
{
    for (QPushButton *b : w->findChildren<QPushButton *>())
        if (b->text() == text)
            return b;
    return nullptr;
}
}

class TstTroubleshootDialog : public QObject {
    Q_OBJECT
private slots:
    void healthySystemSaysSo()
    {
        TroubleshootDialog d("Troubleshoot", [] { return healthy(); }, false);
        QVERIFY(allText(&d).contains("No problems found"));
        QVERIFY(Diagnostics::worstLevel(d.findings()) <= Diagnostics::Finding::Level::Info);
    }

    void setupModeUsesFirstRunWording()
    {
        TroubleshootDialog d("Let's check your system", [] { return healthy(); }, true);
        QVERIFY(allText(&d).contains("Everything Burrow needs is here"));
    }

    void problemsAreShownWithWhatToTry()
    {
        TroubleshootDialog d("Troubleshoot", [] { auto in = healthy(); in.nmRunning = false; return in; }, true);
        QVERIFY(allText(&d).contains("1 thing needs your attention"));
        const QStringList text = allText(&d);
        QVERIFY(std::any_of(text.cbegin(), text.cend(), [](const QString &s) { return s.contains("NetworkManager isn't running"); }));
        QVERIFY(std::any_of(text.cbegin(), text.cend(), [](const QString &s) { return s.contains("What to try") && s.contains("systemctl"); }));
    }

    void worstProblemsComeFirst()
    {
        TroubleshootDialog d("Troubleshoot", [] { auto in = healthy(); in.connectivity = 2; in.mayModifyConnections = "no"; return in; }, false);
        const auto f = d.findings();
        QVERIFY(f.size() >= 3);
        QCOMPARE(f.first().level, Diagnostics::Finding::Level::Problem);
        QCOMPARE(f.last().level, Diagnostics::Finding::Level::Ok);
        for (int i = 1; i < f.size(); ++i)
            QVERIFY(f[i - 1].level >= f[i].level);
    }

    void runAgainSeesFreshState()
    {
        bool running = false;
        TroubleshootDialog d("Troubleshoot", [&running] { auto in = healthy(); in.nmRunning = running; return in; }, false);
        QVERIFY(Diagnostics::worstLevel(d.findings()) == Diagnostics::Finding::Level::Problem);

        running = true; // the user fixed it
        button(&d, "Run checks again")->click();
        QVERIFY(Diagnostics::worstLevel(d.findings()) <= Diagnostics::Finding::Level::Info);
        QVERIFY(allText(&d).contains("No problems found"));
        // and the old result is gone, not stacked underneath
        const QStringList text = allText(&d);
        QVERIFY(!std::any_of(text.cbegin(), text.cend(), [](const QString &s) { return s.contains("NetworkManager isn't running"); }));
    }

    void copyReportPutsItOnTheClipboard()
    {
        TroubleshootDialog d("Troubleshoot", [] { return healthy(); }, false);
        QGuiApplication::clipboard()->clear();
        button(&d, "Copy report")->click();
        const QString text = QGuiApplication::clipboard()->text();
        QVERIFY(text.contains("Burrow diagnostic report"));
        QVERIFY(text.contains("Test OS"));
        QVERIFY(allText(&d).contains("Copied. Paste it where you're asking for help."));
    }
};

QTEST_MAIN(TstTroubleshootDialog)
#include "tst_troubleshootdialog.moc"
