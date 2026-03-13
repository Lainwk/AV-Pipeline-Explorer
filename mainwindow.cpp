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

    //Device Manage Page
    connect(this->device_manage_page,&DeviceManageWidget::set_scaned_devices,
            this,&MainWindow::set_scaned_device);

    //set Device
    connect(this,&MainWindow::set_current_device,
            this->device_manage_page,&ModelWidget::set_selected_device);
    connect(this,&MainWindow::set_current_device,
            this->V4L2_capture_page,&ModelWidget::set_selected_device);

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
    emit this->set_current_device(QString(this->ui->videoDeviceCombo->currentData().toString()),
                                  QString(this->ui->audioDeviceCombo->currentData().toString()));
    this->add_logs(QString("[Setting]current video device:%1, current audio device:%2").arg(this->ui->videoDeviceCombo->currentText()).arg(this->ui->audioDeviceCombo->currentText()));
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
                                   const QStringList & audioDeviceIdList)
{
    qDebug()<<"set all device";
    this->ui->videoDeviceCombo->clear();
        for(int i=0; i<videoDeviceList.size(); i++){
            QString displayText = videoDeviceList.at(i);
            QString devicePath = videoDevicePathList.at(i);
            // 添加显示文本，并设置隐藏数据（Qt::UserRole）
            this->ui->videoDeviceCombo->addItem(displayText, devicePath);
        }
        if(!videoDeviceList.isEmpty()){
            this->ui->videoDeviceCombo->setCurrentIndex(0);
        }

        // 清空音频ComboBox并添加Item（显示文本+隐藏数据）
        this->ui->audioDeviceCombo->clear();
        for(int i=0; i<audioDeviceList.size(); i++){
            QString displayText = audioDeviceList.at(i);
            QString deviceId = audioDeviceIdList.at(i);
            // 添加显示文本，并设置隐藏数据（Qt::UserRole）
            this->ui->audioDeviceCombo->addItem(displayText, deviceId);
        }
        if(!audioDeviceList.isEmpty()){
            this->ui->audioDeviceCombo->setCurrentIndex(0);
        }

    emit this->set_current_device(QString(this->ui->videoDeviceCombo->currentData().toString()),
                                  QString(this->ui->audioDeviceCombo->currentData().toString()));
    this->add_logs(QString("[Setting]current video device:%1, current audio device:%2").arg(this->ui->videoDeviceCombo->currentText()).arg(this->ui->audioDeviceCombo->currentText()));


}

