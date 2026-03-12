#ifndef DEVICEMANAGEWIDGET_H
#define DEVICEMANAGEWIDGET_H

#include <QWidget>
#include <QTreeWidgetItem>
#include <QProcess>
#include <QDateTime>
#include <QTimer>
#include <QDir>

namespace Ui {
class DeviceManageWidget;
}

class DeviceManageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceManageWidget(QWidget *parent = nullptr);
    ~DeviceManageWidget();

private:
    Ui::DeviceManageWidget *ui;

    // 当前选中的设备
    QString selected_video_device;
    QString selected_audio_device;

    void init_connect();

    void scan_video_devices();
    void scan_audio_devices();

    QString execute_command(const QString &command, const QStringList &arguments);

private slots:
    void on_scan_button_clicked();

signals:
    void DeviceManageWidget_Logs(const QString &message);


};

#endif // DEVICEMANAGEWIDGET_H
