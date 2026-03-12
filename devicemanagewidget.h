#ifndef DEVICEMANAGEWIDGET_H
#define DEVICEMANAGEWIDGET_H

#include "modelwidget.h"
#include <QTreeWidgetItem>
#include <QProcess>
#include <QDateTime>
#include <QTimer>
#include <QDir>
#include <QRegularExpression>
#include <QThread>


namespace Ui {
class DeviceManageWidget;
}

class DeviceManageWidget : public ModelWidget
{
    Q_OBJECT

public:
    explicit DeviceManageWidget(QWidget *parent = nullptr);
    ~DeviceManageWidget();

private:
    Ui::DeviceManageWidget *ui;    

    void scan_video_devices();
    void scan_audio_devices();

    virtual void init_connect() override;


private slots:
    void on_scan_button_clicked();

    void on_video_device_double_clicked(QTreeWidgetItem *item, int column);
    void on_audio_device_double_clicked(QTreeWidgetItem *item, int column);

    void on_test_video_button_clicked();
    void on_test_audio_button_clicked();


signals:
    void set_scaned_devices(const QStringList &videoDeviceList,
                            const QStringList &audioDeviceList,
                            const QStringList &videoDevicePathList,
                            const QStringList &audioDeviceIdList);

};

#endif // DEVICEMANAGEWIDGET_H
