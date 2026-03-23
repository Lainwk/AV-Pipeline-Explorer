#include "v4l2capturewidget.h"
#include "ui_v4l2capturewidget.h"

V4L2CaptureWidget::V4L2CaptureWidget(QWidget *parent) : ModelWidget(parent),
                                                        ui(new Ui::V4L2CaptureWidget)
{
    ui->setupUi(this);
    this->ui->pushButton_open_close_device->setProperty("device_state", false);

    this->previewThread = new V4L2PreviewThread();
    this->videoLabel = nullptr;
    this->isVideoLabelInitialized = false;

    this->initVideoLabel();
    this->init_connect();
}

V4L2CaptureWidget::~V4L2CaptureWidget()
{
    delete ui;
    if (this->v4l2_fd >= 0)
    {
        ::close(v4l2_fd);
    }
}

void V4L2CaptureWidget::set_select_device(const selectedDeviceV4L2Params &newCurrentVideoDeviceParams, const QString &audioDevice)
{
    currentVideoDeviceParams = newCurrentVideoDeviceParams;
    selected_audio_device = audioDevice;
    this->update_video_params();
}

void V4L2CaptureWidget::initVideoLabel()
{
    if (this->videoLabel)
    {
        return;
    }

    this->add_local_logs("[V4L2CaptureWidget][Operation] Creating video label...");

    // 创建QLabel
    this->videoLabel = new QLabel(this);
    this->videoLabel->setParent(this->ui->frame_preview);
    this->videoLabel->setGeometry(this->ui->frame_preview->rect());

    // 设置QLabel属性
    this->videoLabel->setAlignment(Qt::AlignCenter);
    this->videoLabel->setScaledContents(false);
    this->videoLabel->setStyleSheet("QLabel { background-color: #222222; color: white; font-size: 16px; }");
    this->videoLabel->setText("无视频信号");


    this->videoLabel->show();

    this->add_local_logs("[V4L2CaptureWidget][Success] Video label created");
}

void V4L2CaptureWidget::init_connect()
{
    connect(this->ui->pushButton_open_close_device, &QPushButton::clicked,
            this, &V4L2CaptureWidget::on_open_close_device_button_clicked);

    connect(this->previewThread, &V4L2PreviewThread::add_Logs,
            this, &V4L2CaptureWidget::add_local_logs);

    connect(this->previewThread, &V4L2PreviewThread::previewImageReady,
            this, &V4L2CaptureWidget::updateVideoFrame,
            Qt::QueuedConnection);

    connect(this->previewThread, &V4L2PreviewThread::paramsWarning,
            this,&V4L2CaptureWidget::showParamsWarnBox);

    connect(this->ui->comboBox_video_params,&QComboBox::currentIndexChanged,
            this,&V4L2CaptureWidget::onVideoParamsComBoboxChanged);

}


QImage V4L2CaptureWidget::convertYuyvToRgb(const uchar *data, int width, int height)
{
    QImage image(width, height, QImage::Format_RGB888);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; x += 2)
        {
            int index = (y * width + x) * 2;

            int y0 = data[index];
            int u = data[index + 1];
            int y1 = data[index + 2];
            int v = data[index + 3];

            int c0 = y0 - 16;
            int c1 = y1 - 16;
            int d = u - 128;
            int e = v - 128;

            int r0 = qBound(0, (298 * c0 + 409 * e + 128) >> 8, 255);
            int g0 = qBound(0, (298 * c0 - 100 * d - 208 * e + 128) >> 8, 255);
            int b0 = qBound(0, (298 * c0 + 516 * d + 128) >> 8, 255);

            int r1 = qBound(0, (298 * c1 + 409 * e + 128) >> 8, 255);
            int g1 = qBound(0, (298 * c1 - 100 * d - 208 * e + 128) >> 8, 255);
            int b1 = qBound(0, (298 * c1 + 516 * d + 128) >> 8, 255);

            image.setPixel(x, y, qRgb(r0, g0, b0));
            if (x + 1 < width)
            {
                image.setPixel(x + 1, y, qRgb(r1, g1, b1));
            }
        }
    }

    return image;
}

QImage V4L2CaptureWidget::convertMjpegToRgb(const uchar *data,int size, int width, int height)
{
    qDebug()<<"data:"<<*data<<"  size:"<<size;
    QImage image;
   // 直接从内存数据加载 JPEG
   if (!image.loadFromData(data,size, "JPEG")) {
       return QImage(); // 加载失败，返回空图像
   }
   // 可选的尺寸验证或缩放
   if (image.width() != width || image.height() != height) {
       image = image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
   }
   // 确保输出为 RGB888
   if (image.format() != QImage::Format_RGB888) {
       image = image.convertToFormat(QImage::Format_RGB888);
   }
   return image;

}

void V4L2CaptureWidget::updateVideoFrame(const uchar *Data,int size, int width, int height)
{
    if (!this->videoLabel)
    {
        return;
    }
    QImage rgbImage;
    switch(this->currentVideoDeviceParams.validParamList
           .at(this->ui->comboBox_video_params->currentIndex()).pixFmt)
    {
        case V4L2_PIX_FMT_YUYV:
            rgbImage = convertYuyvToRgb(Data, width, height);
        break;

        case V4L2_PIX_FMT_MJPEG:
            rgbImage = convertMjpegToRgb(Data, size ,width, height);
    }


    // 缩放图像以适应QLabel大小（保持宽高比）
    QPixmap pixmap = QPixmap::fromImage(rgbImage);
    QPixmap scaledPixmap = pixmap.scaled(this->videoLabel->size(),
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);

    // 显示图像
    this->videoLabel->setPixmap(scaledPixmap);
    this->videoLabel->show();
}

void V4L2CaptureWidget::update_video_params()
{
    bool oldState = this->ui->comboBox_video_params->blockSignals(true);
    if(this->ui->comboBox_video_params->count()>0){
        this->ui->comboBox_video_params->clear();
    }

    for(int i = 0;i<this->currentVideoDeviceParams.validParamList.size();i++){
        this->ui->comboBox_video_params->addItem(QString("分辨率：%1x%2  帧率：%3  格式：%4")
                                                 .arg(this->currentVideoDeviceParams.validParamList.at(i).width)
                                                 .arg(this->currentVideoDeviceParams.validParamList.at(i).height)
                                                 .arg(this->currentVideoDeviceParams.validParamList.at(i).fps)
                                                 .arg(this->getPixFmtString(this->currentVideoDeviceParams.validParamList.at(i).pixFmt)));
    }
    this->ui->comboBox_video_params->setCurrentIndex(0);
    this->ui->comboBox_video_params->blockSignals(oldState);

}

void V4L2CaptureWidget::onVideoParamsComBoboxChanged(int index)
{
    if(this->ui->pushButton_open_close_device->property("device_state").toBool()){
        this->add_local_logs("[V4L2CaptureWidget][Operation] reset video params fail, please close device before reset");
        this->ui->comboBox_video_params->setCurrentIndex(this->lastParamsComboboxIndex);
    } else {
        this->lastParamsComboboxIndex = index;
        this->add_local_logs("[V4L2CaptureWidget][Operation] reset video params success");
        this->previewThread->setParams(this->currentVideoDeviceParams.validParamList.at(index));
        this->add_local_logs(QString("[V4L2CaptureWidget][Operation] :"));
        this->add_local_logs(QString("---- resolution:%1 x %2")
                             .arg(this->currentVideoDeviceParams.validParamList.at(index).width)
                             .arg(this->currentVideoDeviceParams.validParamList.at(index).height));
        this->add_local_logs(QString("---- fps:%1")
                             .arg(this->currentVideoDeviceParams.validParamList.at(index).fps));
        this->add_local_logs(QString("---- pixFmt:%1")
                             .arg(this->getPixFmtString(this->currentVideoDeviceParams.validParamList.at(index).pixFmt)));

    }
}

void V4L2CaptureWidget::showParamsWarnBox(const QString &title, const QString &message)
{
    bool oldState = this->ui->comboBox_video_params->blockSignals(true);
    this->ui->comboBox_video_params->setCurrentIndex(0);
    this->previewThread->setParams(this->currentVideoDeviceParams.validParamList.at(0));
    this->ui->comboBox_video_params->blockSignals(oldState);
    this->showWarningBox(this,title,message);
}

QString V4L2CaptureWidget::format_v4l2_error(const QString &operation, int err_code)
{
    return QString("[Error][V4L2] %1 failure：%2 (erroe code：%3)")
        .arg(operation)
        .arg(strerror(err_code))
        .arg(err_code);
}


void V4L2CaptureWidget::startPreviewThread()
{
    if (!this->videoLabel) {
        add_local_logs("[V4L2CaptureWidget][Error] Video label not initialized");
        return;
    }

    // 验证设备fd有效性
    if (v4l2_fd < 0) {
        add_local_logs("[V4L2CaptureWidget][Error] Invalid V4L2 fd");
        return;
    }

    // 测试设备是否可读
    struct v4l2_capability cap;
    if (ioctl(v4l2_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        add_local_logs(QString("[V4L2CaptureWidget][Error] Device not accessible: %1")
                      .arg(strerror(errno)));
        return;
    }

    this->previewThread->setV4l2Fd(v4l2_fd);
    V4L2Params params = this->currentVideoDeviceParams.validParamList.at(0);
    this->previewThread->setParams(params);

    this->previewThread->start();
    add_local_logs("[V4L2CaptureWidget][Success] start preview thread success");
    emit this->add_Logs("[V4L2CaptureWidget][Operation] start preview");
}


void V4L2CaptureWidget::stopPreviewThread()
{
    this->previewThread->stop_Preview();
    this->previewThread->wait();
    add_local_logs("[V4L2CaptureWidget][Success] stop preview thread");
    emit this->add_Logs("[V4L2CaptureWidget][Operation] stop preview");
}

void V4L2CaptureWidget::on_open_close_device_button_clicked()
{
    qDebug() << "on_open_close_device_button_clicked";
    if (this->ui->pushButton_open_close_device->property("device_state").toBool())
    {
        emit this->add_Logs("[V4L2CaptureWidget][Operation] close V4L2 device");
        this->ui->pushButton_open_close_device->setProperty("device_state", false);
        if (this->currentVideoDeviceParams.selectedVideoDevice.isEmpty())
        {
            this->add_local_logs("[Error] No video device has been selected");
            return;
        }
        if (v4l2_fd < 0)
        {
            this->add_local_logs("[Erroe] no V4L2 device opened");
            return;
        }

        this->add_local_logs(QString("[Operation] try to close device（fd：%1）").arg(v4l2_fd));

        int ret = ::close(v4l2_fd);
        if (ret < 0)
        {
            this->add_local_logs(format_v4l2_error("close device", errno));
        }
        else
        {
            this->add_local_logs("[Success] close device success");
        }

        this->stopPreviewThread();
        v4l2_fd = -1;

        // 重置显示
        if (this->videoLabel)
        {
            this->videoLabel->clear();
            this->videoLabel->setText("无视频信号");
        }

        this->add_local_logs("----------------------------------------");
    }
    else
    {
        emit this->add_Logs("[V4L2CaptureWidget][Operation] open V4L2 device");
        this->ui->pushButton_open_close_device->setProperty("device_state", true);
        if (this->currentVideoDeviceParams.selectedVideoDevice.isEmpty())
        {
            this->add_local_logs("[Error] No video device has been selected");
            return;
        }
        this->add_local_logs(QString("[Operation] try to open device：%1")
                                 .arg(this->currentVideoDeviceParams.selectedVideoDevice));

        execute_command("sudo", QStringList() << "chmod" << "666" << this->currentVideoDeviceParams.selectedVideoDevice);

        //v4l2_fd = open(this->currentVideoDeviceParams.selectedVideoDevice.toUtf8().data(), O_RDWR | O_NONBLOCK);
        v4l2_fd = open(this->currentVideoDeviceParams.selectedVideoDevice.toUtf8().data(), O_RDWR );
        if (v4l2_fd < 0)
        {
            this->add_local_logs(format_v4l2_error("open device", errno));
            this->add_local_logs("[Reason]：1. The device is occupied by another process 2. Insufficient permissions 3. The USBIPD device is not mounted correctly.");
            v4l2_fd = -1;
            return;
        }

        this->startPreviewThread();
        this->add_local_logs(QString("[Success] device opened ：%1").arg(v4l2_fd));
        this->add_local_logs("----------------------------------------");
    }
}

void V4L2CaptureWidget::add_local_logs(const QString &logMessage)
{
    this->ui->textEdit_status->append(logMessage);
}
