#include "ui/TrayController.h"

#include <QAction>
#include <QMenu>
#include <QSignalSpy>
#include <QtTest>

namespace {
Tunnel tunnel(const QString &uuid, const QString &name, Tunnel::State state)
{
    Tunnel t;
    t.uuid = uuid;
    t.id = name;
    t.state = state;
    return t;
}

QStringList texts(const QMenu *m)
{
    QStringList out;
    for (const QAction *a : m->actions())
        out << (a->isSeparator() ? QStringLiteral("---") : a->text());
    return out;
}

QAction *find(const QMenu *m, const QString &text)
{
    for (QAction *a : m->actions())
        if (a->text() == text)
            return a;
    return nullptr;
}
}

class TstTray : public QObject {
    Q_OBJECT
private slots:
    void emptyMenu()
    {
        TrayController tray;
        QCOMPARE(texts(tray.menu()), (QStringList{"Open Burrow", "---", "No connections yet", "---", "Quit"}));
        QVERIFY(!find(tray.menu(), "No connections yet")->isEnabled());
    }

    void menuReflectsState()
    {
        TrayController tray;
        tray.setTunnels({tunnel("a", "Home", Tunnel::State::Connected), tunnel("b", "Work", Tunnel::State::Disconnected),
                         tunnel("c", "Spain", Tunnel::State::Connecting)});
        QVERIFY(find(tray.menu(), "Disconnect Home"));
        QVERIFY(find(tray.menu(), "Connect Work"));
        QAction *busy = find(tray.menu(), "Spain (connecting…)");
        QVERIFY(busy);
        QVERIFY(!busy->isEnabled());
    }

    void rebuildingDoesNotDuplicateEntries()
    {
        TrayController tray;
        const QList<Tunnel> one{tunnel("a", "Home", Tunnel::State::Disconnected)};
        tray.setTunnels(one);
        tray.setTunnels(one);
        tray.setTunnels(one);
        QCOMPARE(texts(tray.menu()), (QStringList{"Open Burrow", "---", "Connect Home", "---", "Quit"}));
    }

    void actionsEmitTheRightSignals()
    {
        TrayController tray;
        tray.setTunnels({tunnel("a", "Home", Tunnel::State::Connected), tunnel("b", "Work", Tunnel::State::Disconnected)});
        QSignalSpy connectSpy(&tray, &TrayController::connectRequested);
        QSignalSpy disconnectSpy(&tray, &TrayController::disconnectRequested);
        QSignalSpy quitSpy(&tray, &TrayController::quitRequested);
        QSignalSpy openSpy(&tray, &TrayController::openRequested);

        find(tray.menu(), "Connect Work")->trigger();
        find(tray.menu(), "Disconnect Home")->trigger();
        find(tray.menu(), "Quit")->trigger();
        find(tray.menu(), "Open Burrow")->trigger();

        QCOMPARE(connectSpy.size(), 1);
        QCOMPARE(connectSpy.first().first().toString(), "b");
        QCOMPARE(disconnectSpy.size(), 1);
        QCOMPARE(disconnectSpy.first().first().toString(), "a");
        QCOMPARE(quitSpy.size(), 1);
        QCOMPARE(openSpy.size(), 1);
    }

    void tooltipNamesConnectedTunnels()
    {
        QVERIFY(TrayController::tooltipFor({}).contains("Not connected"));
        const QString tip = TrayController::tooltipFor({tunnel("a", "Home", Tunnel::State::Connected),
                                                        tunnel("b", "Work", Tunnel::State::Disconnected)});
        QVERIFY(tip.contains("Connected: Home"));
        QVERIFY(!tip.contains("Work"));
    }

    void usesRenamedLabel()
    {
        TrayController tray;
        Tunnel t = tunnel("a", "nm-name", Tunnel::State::Disconnected);
        t.displayName = "My VPN";
        tray.setTunnels({t});
        QVERIFY(find(tray.menu(), "Connect My VPN"));
    }
};

QTEST_MAIN(TstTray)
#include "tst_tray.moc"
