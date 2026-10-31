#include "core/Preferences.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("wgtray");
    app.setApplicationDisplayName(QObject::tr("Burrow"));
    app.setApplicationVersion(PROJECT_VERSION);
    app.setDesktopFileName(WGTRAY_APP_ID); // matches the installed .desktop file, so the shell finds the icon

    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256})
        icon.addFile(QString(":/icons/wgtray-%1.png").arg(size));
    app.setWindowIcon(icon);
    app.setQuitOnLastWindowClosed(false); // the tray keeps us alive; MainWindow quits explicitly when there is no tray

    Theme::apply(Preferences().theme());

    MainWindow window;
    window.show();
    return app.exec();
}
