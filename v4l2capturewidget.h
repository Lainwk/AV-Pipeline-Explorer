#ifndef V4L2CAPTUREWIDGET_H
#define V4L2CAPTUREWIDGET_H

#include "modelwidget.h"
#include <QProcess>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QShowEvent>
#include <QLabel>
#include <QImage>
#include <QPixmap>

#include "v4l2previewthread.h"

namespace Ui
{
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

    QLabel *videoLabel;
    V4L2PreviewThread *previewThread;
    int v4l2_fd = -1;
    bool isVideoLabelInitialized = false;

    QString format_v4l2_error(const QString &operation, int err_code);
    void startPreviewThread();
    void stopPreviewThread();
    void initVideoLabel();

    // YUYV转RGB方法
    QImage convertYuyvToRgb(const uchar *data, int width, int height);

private slots:
    void on_open_close_device_button_clicked();
    void add_local_logs(const QString &logMessage);
    void updateVideoFrame(const uchar *yuyvData, int width, int height);

protected:
    void init_connect();

signals:
    void sendYuyvFrame(const uchar *data, int width, int height);
};

#endif // V4L2CAPTUREWIDGET_H
