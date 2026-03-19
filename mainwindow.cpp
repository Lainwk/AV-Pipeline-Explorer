#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->add_logs("[System]system init");

    //initpages
    this->init_subpage();

    //init connect
    this->init_connect();

    this->add_logs("[System]init success");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::init_subpage()
{
    QWidget *placeholderPage = ui->contentStack->widget(0);
    //set device_manage_page
    this->device_manage_page = new DeviceManageWidget();

    ui->contentStack->removeWidget(placeholderPage);
    ui->contentStack->insertWidget(0, this->device_manage_page);
    qDebug() << "device_manage_page added at index 0";

    this->V4L2_capture_page = new V4L2CaptureWidget();
    placeholderPage = ui->contentStack->widget(1);
    ui->contentStack->removeWidget(placeholderPage);
    ui->contentStack->insertWidget(1, this->V4L2_capture_page);
    qDebug() << "V4L2_Capture_page added at index 0";

    // 删除占位页面
    delete placeholderPage;

    qDebug() << "Total pages in stack:" << ui->contentStack->count();
    this->ui->contentStack->setCurrentIndex(0);
}

void MainWindow::init_connect()
{
    connect(this->ui->navigationList,&QListWidget::currentRowChanged,
            this,&MainWindow::on_navigation_item_clicked);

    connect(this->ui->actionShowConfigPanel, &QAction::toggled,
                this, &MainWindow::on_action_show_config_panel);
    connect(this->ui->actionShowFlowchartPanel, &QAction::toggled,
            this, &MainWindow::on_action_show_flowchart_panel);
    connect(this->ui->actionShowLoggingPanel, &QAction::toggled,
            this, &MainWindow::on_action_show_logging_panel);

    connect(this->ui->videoDeviceCombo,&QComboBox::currentIndexChanged,
            this,&MainWindow::on_select_device_change);
    connect(this->ui->audioDeviceCombo,&QComboBox::currentIndexChanged,
            this,&MainWindow::on_select_device_change);

    connect(this->ui->clearLogButton,&QPushButton::clicked,
            this,&MainWindow::on_clear_log_botton_clicked);


    //other page signal
    //page logs
    connect(this->device_manage_page,&ModelWidget::add_Logs,
            this,&MainWindow::add_logs);

    //set Device
    // set_current_device信号连接
    connect(this,&MainWindow::set_current_device,
            this->device_manage_page,&ModelWidget::set_select_device);
    connect(this,&MainWindow::set_current_device,
            this->V4L2_capture_page,&ModelWidget::set_select_device);

    // 替换DeviceManageWidget的set_scaned_devices信号连接（需同步修改DeviceManageWidget的信号）
    connect(this->device_manage_page,&DeviceManageWidget::set_scaned_devices,
            this,&MainWindow::set_scaned_device);

}

void MainWindow::on_navigation_item_clicked(int index)
{
    this->ui->contentStack->setCurrentIndex(index);
    qDebug()<<"page index change to "<< index;

}

void MainWindow::on_action_show_config_panel(bool checked)
{
    this->ui->configDock->setVisible(checked);
    this->add_logs(QString("[MainWindow]config_panel: %1").arg(checked ? "show" : "hide"));
}

void MainWindow::on_action_show_flowchart_panel(bool checked)
{
    this->ui->flowchartDock->setVisible(checked);
    this->add_logs(QString("[MainWindow]flowchart_panel: %1").arg(checked ? "show" : "hide"));
}

void MainWindow::on_action_show_logging_panel(bool checked)
{
    this->ui->loggingDock->setVisible(checked);
    this->add_logs(QString("[MainWindow]logging_panel: %1").arg(checked ? "show" : "hide"));
}

void MainWindow::on_clear_log_botton_clicked()
{
    this->ui->logTextEdit->clear();
}

void MainWindow::on_select_device_change(int index)
{
    Q_UNUSED(index);
    this->add_logs("[Setting]current device changed");

    // 关键：根据选中的设备路径，构建selectedDeviceV4L2Params结构体
    QString videoDevPath = this->ui->videoDeviceCombo->currentData().toString();
    QString audioDev = this->ui->audioDeviceCombo->currentData().toString();

    // （TODO：从设备扫描结果中匹配videoDevPath对应的参数，填充结构体）
    selectedDeviceV4L2Params devParams;
    devParams.selectedVideoDevice = videoDevPath;
    QVariant var = this->ui->videoDeviceCombo->currentData();
    devParams = var.value<selectedDeviceV4L2Params>();

    // 发送结构体信号
    emit this->set_current_device(devParams, audioDev);

    this->add_logs(QString("[Setting]current video device:%1, current audio device:%2")
                   .arg(this->ui->videoDeviceCombo->currentText())
                   .arg(this->ui->audioDeviceCombo->currentText()));
}


void MainWindow::add_logs(const QString &log_string)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logMessage = QString("[%1] %2").arg(timestamp).arg(log_string);
    ui->logTextEdit->append(logMessage);
    qDebug() << logMessage;

}

void MainWindow::set_scaned_device(const QStringList &videoDeviceList,
                                   const QStringList &audioDeviceList,
                                   const QStringList &videoDevicePathList,
                                   const QStringList &audioDeviceIdList,
                                   const QList<selectedDeviceV4L2Params> &videoDeviceParamsList)
{
    qDebug()<<"set all device";
    this->ui->videoDeviceCombo->clear();
    for(int i=0; i<videoDeviceList.size(); i++){
        QString displayText = videoDeviceList.at(i);
        QString devicePath = videoDevicePathList.at(i);
        QVariant var;
        var.setValue(videoDeviceParamsList.at(i));
        this->ui->videoDeviceCombo->addItem(displayText, var); // 替换原devicePath为结构体
    }
    if(!videoDeviceList.isEmpty()){
        this->ui->videoDeviceCombo->setCurrentIndex(0);
    }

    this->ui->audioDeviceCombo->clear();
    for(int i=0; i<audioDeviceList.size(); i++){
        QString displayText = audioDeviceList.at(i);
        QString deviceId = audioDeviceIdList.at(i);
        this->ui->audioDeviceCombo->addItem(displayText, deviceId);
    }
    if(!audioDeviceList.isEmpty()){
        this->ui->audioDeviceCombo->setCurrentIndex(0);
    }

    // 发送初始选中设备的参数
    if(!videoDeviceParamsList.isEmpty()){
        emit this->set_current_device(videoDeviceParamsList.at(0),
                                      this->ui->audioDeviceCombo->currentData().toString());
    }
    this->add_logs(QString("[Setting]current video device:%1, current audio device:%2")
                   .arg(this->ui->videoDeviceCombo->currentText())
                   .arg(this->ui->audioDeviceCombo->currentText()));
}

