#include "ui/mainwindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME"))
        qputenv("QT_QPA_PLATFORMTHEME", "kde");

    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("LGL Papercutter"));
    application.setApplicationVersion(QStringLiteral("0.1.0"));
    application.setOrganizationName(QStringLiteral("LinuxGamerLife"));
    application.setDesktopFileName(
        QStringLiteral("com.linuxgamerlife.lgl-papercutter"));

    QFont font = application.font();
    font.setPointSize(font.pointSize() + 1);
    application.setFont(font);

    papercutter::MainWindow window;
    window.show();
    return application.exec();
}
