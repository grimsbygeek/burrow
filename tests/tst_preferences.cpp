#include "core/Preferences.h"

#include <QtTest>

class TstPreferences : public QObject {
    Q_OBJECT
private slots:
    void themeDefaultsToSystemAndPersists()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.ini");

        QCOMPARE(Preferences(path).theme(), Preferences::Theme::System);

        Preferences(path).setTheme(Preferences::Theme::Dark);
        QCOMPARE(Preferences(path).theme(), Preferences::Theme::Dark);

        Preferences(path).setTheme(Preferences::Theme::Light);
        QCOMPARE(Preferences(path).theme(), Preferences::Theme::Light);

        Preferences(path).setTheme(Preferences::Theme::System);
        QCOMPARE(Preferences(path).theme(), Preferences::Theme::System);
    }

    void layoutDefaultsToSimpleAndPersists()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.ini");

        QVERIFY(Preferences(path).simpleLayout());
        Preferences(path).setSimpleLayout(false);
        QVERIFY(!Preferences(path).simpleLayout());
        Preferences(path).setSimpleLayout(true);
        QVERIFY(Preferences(path).simpleLayout());
    }

    void closeToTrayDefaultsOnAndPersists()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.ini");

        QVERIFY(Preferences(path).closeToTray());
        Preferences(path).setCloseToTray(false);
        QVERIFY(!Preferences(path).closeToTray());
    }

    void setupCheckRunsOnceUntilReset()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.ini");

        QVERIFY(!Preferences(path).setupDone());
        Preferences(path).setSetupDone(true);
        QVERIFY(Preferences(path).setupDone());
        Preferences(path).setSetupDone(false);
        QVERIFY(!Preferences(path).setupDone());
    }

    void unknownValueFallsBackToSystem()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.ini");
        QSettings(path, QSettings::IniFormat).setValue("appearance/theme", "neon");
        QCOMPARE(Preferences(path).theme(), Preferences::Theme::System);
    }
};

QTEST_GUILESS_MAIN(TstPreferences)
#include "tst_preferences.moc"
