#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->add_logs("start init system");

    //initpages
    this->init_subpage();

    //init connect
    this->init_connect();

    this->add_logs("system work");

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::init_subpage()
{
    //set device_manage_page
    this->device_manage_page = new DeviceManageWidget();
    // 获取第一个占位页面
    QWidget *placeholderPage = ui->contentStack->widget(0);

    // 从堆栈中移除占位页面
    ui->contentStack->removeWidget(placeholderPage);

    // 在索引0的位置插入真实的设备管理页面
    ui->contentStack->insertWidget(0, this->device_manage_page);
    qDebug() << "Device management page added at index 0";

    // 删除占位页面
    delete placeholderPage;

    qDebug() << "Total pages in stack:" << ui->contentStack->count();
    this->ui->contentStack->setCurrentIndex(0);
}

void MainWindow::init_connect()
{
    connect(this->ui->navigationList,&QListWidget::currentRowChanged,this,&MainWindow::on_navigation_item_clicked);

    connect(ui->actionShowConfigPanel, &QAction::toggled,
                this, &MainWindow::on_action_show_config_panel);
    connect(ui->actionShowFlowchartPanel, &QAction::toggled,
            this, &MainWindow::on_action_show_flowchart_panel);
    connect(ui->actionShowLoggingPanel, &QAction::toggled,
            this, &MainWindow::on_action_show_logging_panel);

    //other page signal
    //Device Manage page
    connect(this->device_manage_page,&DeviceManageWidget::DeviceManageWidget_Logs,this,&MainWindow::add_logs);

}

void MainWindow::on_navigation_item_clicked(int index)
{
    this->ui->contentStack->setCurrentIndex(index);
    qDebug()<<"page index change to "<< index;

}

void MainWindow::on_action_show_config_panel(bool checked)
{
    this->ui->configDock->setVisible(checked);
    this->add_logs(QString("config_panel: %1").arg(checked ? "show" : "hide"));
}

void MainWindow::on_action_show_flowchart_panel(bool checked)
{
    this->ui->flowchartDock->setVisible(checked);
    this->add_logs(QString("flowchart_panel: %1").arg(checked ? "show" : "hide"));
}

void MainWindow::on_action_show_logging_panel(bool checked)
{
    this->ui->loggingDock->setVisible(checked);
    this->add_logs(QString("logging_panel: %1").arg(checked ? "show" : "hide"));
}

void MainWindow::add_logs(QString log_string)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logMessage = QString("[%1] %2").arg(timestamp).arg(log_string);
    ui->logTextEdit->append(logMessage);
    qDebug() << logMessage;

}

void MainWindow::set_scaned_device(QStringList videoDeviceList, QStringList audioDeviceList)
{
    qDebug()<<"set all device";
    this->ui->videoDeviceCombo->clear();
    this->ui->videoDeviceCombo->addItems(videoDeviceList);
    this->ui->videoDeviceCombo->setCurrentIndex(0);
    this->select_video_device = this->ui->videoDeviceCombo->currentText();
    qDebug()<<

    for (const QString &videoDevice: videoDeviceList) {
        this->ui->videoDeviceCombo->addItem(videoDevice);
    }
    this->ui->videoDeviceCombo->setCurrentIndex(0);

}

