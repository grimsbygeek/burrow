#include "ui/NewConnectionDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QtTest>

namespace {
const QString kPriv = QStringLiteral("YAnz5TF+lXXJte14tji3zlMNq+hd2rYUIgJBgB3fBmk=");
const QString kPub = QStringLiteral("Y2FD6uTgrq+/bbBAXCNOtE+PGk8Papoh7qaWBpQUXn0=");
const QString kServer = QStringLiteral("xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=");

QLineEdit *edit(QWidget *w, const QString &placeholder)
{
    for (QLineEdit *e : w->findChildren<QLineEdit *>()) {
        if (e->placeholderText() == placeholder)
            return e;
    }
    return nullptr;
}
}

class TstNewConnectionDialog : public QObject {
    Q_OBJECT

    static QPushButton *createButton(NewConnectionDialog &d)
    {
        return d.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    }
    static void fillManual(NewConnectionDialog &d)
    {
        d.findChild<QTabWidget *>()->setCurrentIndex(1);
        edit(&d, "For example: Home, Work, Spain")->setText("Spain");
        edit(&d, "Paste it, or press Generate")->setText(kPriv);
        edit(&d, "10.0.0.2/24")->setText("10.2.0.2/32");
        edit(&d, "The server's public key")->setText(kServer);
        edit(&d, "vpn.example.com:51820")->setText("185.1.2.3:51820");
    }

private slots:
    void startsWithCreateDisabled()
    {
        NewConnectionDialog d({});
        QVERIFY(!createButton(d)->isEnabled());
    }

    void manualEntryProducesConfig()
    {
        NewConnectionDialog d({});
        fillManual(d);
        QVERIFY(createButton(d)->isEnabled());
        QCOMPARE(d.name(), "Spain");
        QCOMPARE(d.config().privateKey, kPriv);
        QCOMPARE(d.config().peers.size(), 1);
        QCOMPARE(d.config().peers[0].endpoint, "185.1.2.3:51820");
        QCOMPARE(d.config().peers[0].allowedIps, (QStringList{"0.0.0.0/0", "::/0"})); // the default
        QCOMPARE(d.config().peers[0].persistentKeepalive, 25);                         // the default
    }

    void showsPublicKeyForPrivateKey()
    {
        NewConnectionDialog d({});
        fillManual(d);
        QCOMPARE(edit(&d, "Appears once you have a private key")->text(), kPub);
    }

    void generateFillsPrivateKey()
    {
        NewConnectionDialog d({});
        d.findChild<QTabWidget *>()->setCurrentIndex(1);
        QLineEdit *priv = edit(&d, "Paste it, or press Generate");
        QVERIFY(priv->text().isEmpty());
        for (QPushButton *b : d.findChildren<QPushButton *>()) {
            if (b->text() == "Generate")
                b->click();
        }
        QVERIFY(!priv->text().isEmpty());
        QVERIFY(!edit(&d, "Appears once you have a private key")->text().isEmpty());
    }

    void rejectsDuplicateNamesAndBadInput()
    {
        NewConnectionDialog d({"Spain"});
        fillManual(d);
        QVERIFY(!createButton(d)->isEnabled()); // name already used
        edit(&d, "For example: Home, Work, Spain")->setText("spain2");
        QVERIFY(createButton(d)->isEnabled());

        edit(&d, "vpn.example.com:51820")->setText("nohost");
        QVERIFY(!createButton(d)->isEnabled());
        QVERIFY(!d.findChildren<QLabel *>().isEmpty());
    }

    void blankNameIsRejected()
    {
        NewConnectionDialog d({});
        fillManual(d);
        edit(&d, "For example: Home, Work, Spain")->setText("   ");
        QVERIFY(!createButton(d)->isEnabled());
    }
};

QTEST_MAIN(TstNewConnectionDialog)
#include "tst_newconnectiondialog.moc"
