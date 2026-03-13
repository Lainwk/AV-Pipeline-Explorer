#ifndef V4L2CAPTUREWIDGET_H
#define V4L2CAPTUREWIDGET_H

#include "modelwidget.h"
#include <QProcess>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>
// 引入V4L2核心头文件（WSL/Linux下需确保安装libv4l-dev）
#include <linux/videodev2.h>
#include <fcntl.h>      // open/close
#include <unistd.h>     // close
#include <errno.h>      // errno
#include <string.h>     // strerror
#include <QFrame>


#include "v4l2previewthread.h"
#include "openglvideowidget.h"

namespace Ui {
class V4L2CaptureWidget;
}

class V4L2CaptureWidget : public ModelWidget
{
    Q_OBJECT

public:
    explicit V4L2CaptureWidget(QWidget *parent = nullptr);
    ~V4L2CaptureWidget();

private:
    Ui::V4L2CaptureWidget *ui;

    OpenGLVideoWidget* openGLWidget;
    V4L2PreviewThread* previewThread;
    int v4l2_fd = -1;          // V4L2设备文件描述符（-1表示未打开）

    // 错误信息格式化（将errno转为可读字符串）
    QString format_v4l2_error(const QString &operation, int err_code);

    // 启动预览线程
    void startPreviewThread();
    // 停止预览线程
    void stopPreviewThread();

private slots:
    void on_open_close_device_button_clicked();
    void add_local_logs(const QString &logMessage);

    // 接收预览图像并显示
    void show_Preview_Image(const QImage &image);

    // ModelWidget interface
protected:
    void init_connect();

signals:
    void sendYuyvFrame(const uchar *data, int width, int height);


};

#endif // V4L2CAPTUREWIDGET_H
