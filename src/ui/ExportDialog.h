#pragma once

#include <QDialog>

class QLabel;
class QPushButton;

// Shows how to take a connection elsewhere: save a .conf file, or scan a QR code with a phone.
// The text contains the private key, so the QR code stays hidden until asked for.
class ExportDialog : public QDialog {
    Q_OBJECT
public:
    ExportDialog(const QString &connectionName, const QString &confText, QWidget *parent = nullptr);

private:
    void saveToFile();
    void toggleQr();

    QString m_name;
    QString m_text;
    QImage m_qr;
    QPushButton *m_qrButton;
    QLabel *m_qrLabel;
    QLabel *m_qrHint;
    QLabel *m_status;
};
