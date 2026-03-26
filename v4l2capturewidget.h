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
#include <QComboBox>
#include "v4l2previewthread.h"
#include "v4l2recordthread.h"

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

public slots:
    void set_select_device(const selectedDeviceV4L2Params &newCurrentVideoDeviceParams, const QString &audioDevice) override;

private:
    Ui::V4L2CaptureWidget *ui;

    QLabel *videoLabel;
    V4L2PreviewThread *previewThread;
    V4L2RecordThread *recordThread;

    int v4l2_fd = -1;
    bool isVideoLabelInitialized = false;
    int lastParamsComboboxIndex = 0;

    QString format_v4l2_error(const QString &operation, int err_code);
    void startPreviewThread();
    void stopPreviewThread();
    void initVideoLabel();

    // YUYV转RGB方法
    QImage convertYuyvToRgb(const uchar *data, int width, int height);
    QImage convertMjpegToRgb(const uchar *data,int size, int width, int height);

private slots:
    void on_open_close_device_button_clicked();
    void add_local_logs(const QString &logMessage);
    void updateVideoFrame(const uchar *yuyvData,int size, int width, int height);
    void update_video_params();
    void onVideoParamsComBoboxChanged(int index);
    void showParamsWarnBox(const QString &title,const QString &message);
    void startRecordButtonClicked();
    void stopRecordButtonClicked();
    void captureFrameButtonClicked();

protected:
    void init_connect() override;

signals:
    void startRecordThread(const QString &filePath,const V4L2Params &newParams, EncodeFormat format);
    void stopRecordThread();


};

#endif // V4L2CAPTUREWIDGET_H
