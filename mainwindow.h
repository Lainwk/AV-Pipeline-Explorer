#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QDateTime>
#include <QFileDialog>
#include <QDir>

//other widget
#include "modelwidget.h"
#include "devicemanagewidget.h"
#include "v4l2capturewidget.h"

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
    V4L2CaptureWidget* V4L2_capture_page;

    //function
    void init_subpage();
    void init_connect();
    void ensureDefaultStorageDirExists();

private slots:
    void on_navigation_item_clicked(int index);    //change page
    void on_action_show_config_panel(bool checked);
    void on_action_show_flowchart_panel(bool checked);
    void on_action_show_logging_panel(bool checked);
    void on_clear_log_botton_clicked();

    void on_select_device_change(int index);

    void add_logs(const QString &log_string);

    //page signal slot
    void set_scaned_device(const QStringList &videoDeviceList,
                           const QStringList &audioDeviceList,
                           const QStringList &videoDevicePathList,
                           const QStringList & audioDeviceIdList,
                           const QList<selectedDeviceV4L2Params> &videoDeviceParamsList);

    void setOutputPath();

signals:
    void set_current_device(const selectedDeviceV4L2Params &videoParams, const QString &audioDevice);
    void setNewOutputPath(const QString &newFilePath);

};
#endif // MAINWINDOW_H
