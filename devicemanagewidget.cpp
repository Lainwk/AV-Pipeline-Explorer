#include "devicemanagewidget.h"
#include "ui_devicemanagewidget.h"

DeviceManageWidget::DeviceManageWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DeviceManageWidget)
{
    ui->setupUi(this);

    this->init_connect();
}

DeviceManageWidget::~DeviceManageWidget()
{
    delete ui;
}

void DeviceManageWidget::init_connect()
{
    connect(this->ui->scanButton,&QPushButton::clicked,this,&DeviceManageWidget::on_scan_button_clicked);
}

void DeviceManageWidget::scan_video_devices()
{
    emit this->DeviceManageWidget_Logs("start scan video device");

    //scan Dir video file
    QDir devDir("/dev");

    /*
     * entryList(...)：列出/dev目录下符合条件的文件：
        QStringList() << "video*"：筛选以video开头的文件（通配符*匹配任意后缀）；
        QDir::System：只筛选系统文件（设备文件属于系统文件，排除普通文件 / 目录）；
        最终videoDevices会得到类似["video0", "video1"]的设备名列表。
     */
    QStringList videoDeviceList = devDir.entryList(QStringList() << "video*", QDir::System);

    if (videoDeviceList.isEmpty())
    {
        emit this->DeviceManageWidget_Logs("No video device was found");
        return;
    }

    // 遍历每个设备
        for (const QString &device : videoDeviceList)
        {
            QString devicePath = "/dev/" + device;

            // 创建树形项
            QTreeWidgetItem *item = new QTreeWidgetItem(ui->videoDeviceTree);
            item->setData(0, Qt::UserRole, devicePath); // 保存设备路径

            // 尝试获取设备信息
            QString deviceInfo = execute_command("v4l2-ctl",
                                                 QStringList() << "--device=" + devicePath << "--info");

            if (!deviceInfo.isEmpty())
            {
                // 解析设备名称（Card type）
                QString cardName = "unknown device";
                if (deviceInfo.contains("Card type"))
                {
                    int start = deviceInfo.indexOf("Card type") + 11;
                    int end = deviceInfo.indexOf('\n', start);
                    cardName = deviceInfo.mid(start, end - start).trimmed();
                    // 移除多余的空格
                    cardName = cardName.simplified();
                }

                // 解析驱动名称
                QString driverName = "unknown";
                if (deviceInfo.contains("Driver name"))
                {
                    int start = deviceInfo.indexOf("Driver name") + 13;
                    int end = deviceInfo.indexOf('\n', start);
                    driverName = deviceInfo.mid(start, end - start).trimmed();
                }

                // 解析总线信息（判断是否为USB/IP设备）
                QString busInfo = "";
                bool isUsbIp = false;
                if (deviceInfo.contains("Bus info"))
                {
                    int start = deviceInfo.indexOf("Bus info") + 10;
                    int end = deviceInfo.indexOf('\n', start);
                    busInfo = deviceInfo.mid(start, end - start).trimmed();
                    isUsbIp = busInfo.contains("vhci");
                }

                // 设置显示文本：设备路径 (设备名称)
                QString displayText = QString("%1 (%2)").arg(devicePath).arg(cardName);
                item->setText(0, displayText);

                // 设置驱动信息，如果是USB/IP设备则标注
                if (isUsbIp)
                {
                    item->setText(1, QString("%1 [USB/IP]").arg(driverName));
                    item->setIcon(0, QIcon::fromTheme("network-wireless"));
                }
                else
                {
                    item->setText(1, driverName);
                }

                item->setText(2, "ready");
                item->setIcon(2, QIcon::fromTheme("dialog-ok"));

                emit this->DeviceManageWidget_Logs(QString("find video device: %1 - %2").arg(devicePath).arg(cardName));
                if (isUsbIp)
                {
                    emit this->DeviceManageWidget_Logs(QString("  └─ devices shared via USB/IP"));
                }
            }
            else
            {
                item->setText(0, devicePath);
                item->setText(1, "unknown");
                item->setText(2, "fail access");
                item->setIcon(2, QIcon::fromTheme("dialog-error"));

                emit this->DeviceManageWidget_Logs(QString("find video device: %1 (fail access device details)").arg(devicePath));
            }
        }

    emit this->DeviceManageWidget_Logs(QString("total find audio devices %1 ").arg(ui->audioDeviceTree->topLevelItemCount()));

}

void DeviceManageWidget::scan_audio_devices()
{
    emit this->DeviceManageWidget_Logs("start scan audio device");

    // 使用 arecord -l 列出录音设备
    QString output = execute_command("arecord", QStringList() << "-l");

    if (output.isEmpty() || output.contains("no soundcards found"))
    {
        emit this->DeviceManageWidget_Logs("no audio input device was found. Adding the default device");

        // 添加默认的PulseAudio设备
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->audioDeviceTree);
        item->setText(0, "default (PulseAudio default device)");
        item->setText(1, "PulseAudio");
        item->setText(2, "no test");
        item->setData(0, Qt::UserRole, "default");

        emit this->DeviceManageWidget_Logs("add default adudio device: default (PulseAudio)");
        return;
    }

    // 解析输出
    // 格式示例：card 0: PCH [HDA Intel PCH], device 0: ALC269VC Analog [ALC269VC Analog]
    QStringList lines = output.split('\n');
    for (const QString &line : lines)
    {
        if (line.contains("card") && line.contains("device"))
        {
            QTreeWidgetItem *item = new QTreeWidgetItem(ui->audioDeviceTree);

            // 提取卡号
            int cardStart = line.indexOf("card") + 5;
            int cardEnd = line.indexOf(':', cardStart);
            QString cardNum = line.mid(cardStart, cardEnd - cardStart).trimmed();

            // 提取设备号
            int devStart = line.indexOf("device") + 7;
            int devEnd = line.indexOf(':', devStart);
            QString devNum = line.mid(devStart, devEnd - devStart).trimmed();

            // 提取卡名称（第一个方括号中的内容）
            QString cardName = "unknown audio card";
            int nameStart = line.indexOf('[');
            int nameEnd = line.indexOf(']', nameStart);
            if (nameStart != -1 && nameEnd != -1)
            {
                cardName = line.mid(nameStart + 1, nameEnd - nameStart - 1);
            }

            // 提取设备名称（第二个方括号中的内容）
            QString deviceName = "";
            int devNameStart = line.indexOf('[', nameEnd + 1);
            int devNameEnd = line.indexOf(']', devNameStart);
            if (devNameStart != -1 && devNameEnd != -1)
            {
                deviceName = line.mid(devNameStart + 1, devNameEnd - devNameStart - 1);
            }

            // 构建显示文本：hw:卡号,设备号 (设备名称)
            QString hwDevice = QString("hw:%1,%2").arg(cardNum).arg(devNum);
            QString displayText;
            if (!deviceName.isEmpty())
            {
                displayText = QString("%1 (%2)").arg(hwDevice).arg(deviceName);
            }
            else
            {
                displayText = QString("%1 (%2)").arg(hwDevice).arg(cardName);
            }

            item->setText(0, displayText);
            item->setText(1, "ALSA");
            item->setText(2, "no test");
            item->setData(0, Qt::UserRole, hwDevice);

            emit this->DeviceManageWidget_Logs(QString("find audio device: %1 - %2").arg(hwDevice).arg(deviceName.isEmpty() ? cardName : deviceName));
        }
    }

    // 如果没有找到设备，添加默认设备
    if (ui->audioDeviceTree->topLevelItemCount() == 0)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->audioDeviceTree);
        item->setText(0, "default (PulseAudio default device)");
        item->setText(1, "PulseAudio");
        item->setText(2, "no test");
        item->setData(0, Qt::UserRole, "default");

        emit this->DeviceManageWidget_Logs("add default audio device: default");
    }

    emit this->DeviceManageWidget_Logs(QString("total find %1 audio devices").arg(ui->audioDeviceTree->topLevelItemCount()));
}

QString DeviceManageWidget::execute_command(const QString &command, const QStringList &arguments)
{
QProcess process;
process.start(command, arguments);

if (!process.waitForFinished(3000))
{
    return QString();
}

QString output = process.readAllStandardOutput();
if (output.isEmpty())
{
        output = process.readAllStandardError();
    }

    return output;
}

void DeviceManageWidget::on_scan_button_clicked()
{
    emit this->DeviceManageWidget_Logs("start scan device");

    // 清空现有设备列表
    ui->videoDeviceTree->clear();
    ui->audioDeviceTree->clear();

    // 扫描视频设备
    scan_video_devices();

    // 扫描音频设备
    scan_audio_devices();

    // 更新设备总数
    int totalDevices = ui->videoDeviceTree->topLevelItemCount() +
            ui->audioDeviceTree->topLevelItemCount();
    ui->deviceCountValue->setText(QString::number(totalDevices));

    // 完成
    emit this->DeviceManageWidget_Logs("scan device over");

}
