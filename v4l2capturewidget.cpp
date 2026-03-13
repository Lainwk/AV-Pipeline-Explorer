#include "v4l2capturewidget.h"
#include "ui_v4l2capturewidget.h"

V4L2CaptureWidget::V4L2CaptureWidget(QWidget *parent) :
    ModelWidget(parent),
    ui(new Ui::V4L2CaptureWidget)
{
    ui->setupUi(this);
    this->ui->pushButton_open_close_device->setProperty("device_state",false);

    this->previewThread = new V4L2PreviewThread();
    this->openGLWidget = new OpenGLVideoWidget(this);
    //set openGLWidget
    this->openGLWidget->setParent(this->ui->frame_preview);
    this->openGLWidget->setGeometry(this->ui->frame_preview->rect());

    this->init_connect();
}

V4L2CaptureWidget::~V4L2CaptureWidget()
{
    delete ui;
    if(this->v4l2_fd>=0){
        ::close(v4l2_fd);
    }
}

void V4L2CaptureWidget::init_connect()
{
    connect(this->ui->pushButton_open_close_device,&QPushButton::clicked,
            this,&V4L2CaptureWidget::on_open_close_device_button_clicked);

//    connect(this->ui->frame_preview, &QFrame::resize,
//            this->openGLWidget,&QOpenGLWidget::setGeometry);

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

}

void V4L2CaptureWidget::stopPreviewThread()
{

}

void V4L2CaptureWidget::on_open_close_device_button_clicked()
{
    qDebug()<<"on_open_close_device_button_clicked";
    if(this->ui->pushButton_open_close_device->property("device_state").toBool()){
        emit this->add_Logs("[V4L2CaptureWidget][Operation] close V4L2 device");
        this->ui->pushButton_open_close_device->setProperty("device_state",false);
        if(this->selected_video_device.isEmpty()){
            this->add_local_logs("[Error] No video device has been selected");
            return;
        }
        if (v4l2_fd < 0) {
            this->add_local_logs("[Erroe] no V4L2 device opened");
            return;
        }

        this->add_local_logs(QString("[Operation] try to close device（fd：%1）").arg(v4l2_fd));

        // 底层关闭设备
        int ret = ::close(v4l2_fd);
        if (ret < 0) {
            this->add_local_logs(format_v4l2_error("close device", errno));
        } else {
            this->add_local_logs("[Success] close device success");
        }

        // 重置状态
        v4l2_fd = -1;
        this->add_local_logs("----------------------------------------");


    } else {
        emit this->add_Logs("[V4L2CaptureWidget][Operation] open V4L2 device");
        this->ui->pushButton_open_close_device->setProperty("device_state",true);
        if(this->selected_video_device.isEmpty()){
            this->add_local_logs("[Error] No video device has been selected");
            return;
        }
        this->add_local_logs(QString("[Operation] try to open device：%1").arg(selected_video_device));

        // 提权（避免权限不足）
        execute_command("sudo", QStringList() << "chmod" << "666" << selected_video_device);

        // 底层V4L2打开设备（O_RDWR：读写模式，O_NONBLOCK：非阻塞）
        v4l2_fd = open(selected_video_device.toUtf8().data(), O_RDWR | O_NONBLOCK);
        if (v4l2_fd < 0) {
            // 错误处理：输出详细原因
            this->add_local_logs(format_v4l2_error("open device", errno));
            this->add_local_logs("[Reason]：1. The device is occupied by another process 2. Insufficient permissions 3. The USBIPD device is not mounted correctly.");
            v4l2_fd = -1; // 重置fd
            return;
        }

        // 5. 打开成功：更新状态+日志+按钮
        this->add_local_logs(QString("[Success] device opened ：%1").arg(v4l2_fd));
        this->add_local_logs("----------------------------------------");


    }
}

void V4L2CaptureWidget::add_local_logs(const QString &logMessage)
{
    this->ui->textEdit_status->append(logMessage);
}

void V4L2CaptureWidget::show_Preview_Image(const QImage &image)
{

}




