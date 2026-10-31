#include "core/AliasStore.h"

#include <QtTest>

class TstAliasStore : public QObject {
    Q_OBJECT
private slots:
    void storesTrimsAndClears()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("aliases.ini");

        {
            AliasStore store(path);
            QCOMPARE(store.alias("u1"), QString());
            store.setAlias("u1", "  Home  ");
            QCOMPARE(store.alias("u1"), "Home");
        }
        {
            AliasStore reopened(path); // persisted
            QCOMPARE(reopened.alias("u1"), "Home");
            QCOMPARE(reopened.alias("u2"), QString());
            reopened.setAlias("u1", "   ");
            QCOMPARE(reopened.alias("u1"), QString());
        }
    }
};

QTEST_GUILESS_MAIN(TstAliasStore)
#include "tst_aliasstore.moc"
