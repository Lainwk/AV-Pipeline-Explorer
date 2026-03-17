#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    qRegisterMetaType<selectedDeviceV4L2Params>("selectedDeviceV4L2Params");
    qRegisterMetaType<QList<selectedDeviceV4L2Params>>("QList<selectedDeviceV4L2Params>");

    w.show();
    return a.exec();
}
