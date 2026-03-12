#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QDateTime>

//other widget
#include "modelwidget.h"
#include "devicemanagewidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    //element
    Ui::MainWindow *ui;

    //other pages
    DeviceManageWidget* device_manage_page;

    //function
    void init_subpage();
    void init_connect();

private slots:
    void on_navigation_item_clicked(int index);    //change page
    void on_action_show_config_panel(bool checked);
    void on_action_show_flowchart_panel(bool checked);
    void on_action_show_logging_panel(bool checked);

    void on_select_device_change(int index);

    void add_logs(const QString &log_string);

    //page signal slot
    void set_scaned_device(const QStringList &videoDeviceList,
                           const QStringList &audioDeviceList,
                           const QStringList &videoDevicePathList,
                           const QStringList & audioDeviceIdList);

signals:
    void set_current_device(const QString &videoDevice, const QString &audioDevice);

};
#endif // MAINWINDOW_H
