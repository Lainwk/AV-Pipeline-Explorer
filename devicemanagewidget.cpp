#include "devicemanagewidget.h"
#include "ui_devicemanagewidget.h"

DeviceManageWidget::DeviceManageWidget(QWidget *parent) :
    ModelWidget(parent),
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
    connect(this->ui->scanButton,&QPushButton::clicked,
            this,&DeviceManageWidget::on_scan_button_clicked);
    connect(this->ui->videoDeviceTree,&QTreeWidget::itemDoubleClicked,
            this,&DeviceManageWidget::on_video_device_double_clicked);
    connect(this->ui->audioDeviceTree,&QTreeWidget::itemDoubleClicked,
            this,&DeviceManageWidget::on_audio_device_double_clicked);
    connect(this->ui->testVideoButton,&QPushButton::clicked,
            this,&DeviceManageWidget::on_test_video_button_clicked);
    connect(this->ui->testAudioButton,&QPushButton::clicked,
            this,&DeviceManageWidget::on_test_audio_button_clicked);

}

void DeviceManageWidget::scan_video_devices()
{
    emit this->add_Logs("[DeviceManageWidget]start scan video device (USBIPD + WSL)");

    QDir devDir("/dev");
    // 只匹配video数字设备（排除伪设备）
    // 步骤1：先获取/dev下所有System类型的文件（包含video*）
    QStringList allSystemFiles = devDir.entryList(QDir::System);
    // 步骤2：用正则过滤出"video+数字"的设备（如video0/video1）
    QRegularExpression videoRegex("^video\\d+$"); // 严格匹配：以video开头，后跟数字，无其他字符
    QStringList videoDeviceList;
    for (const QString &file : allSystemFiles) {
        QRegularExpressionMatch match = videoRegex.match(file);
        if (match.hasMatch()) {
            videoDeviceList.append(file);
        }
    }

    if (videoDeviceList.isEmpty())
    {
        emit this->add_Logs("[DeviceManageWidget]No video device was found in /dev/");
        return;
    }

    for (const QString &device : videoDeviceList)
    {
        QString devicePath = "/dev/" + device;

        // ========== 核心修复：替换--query-capabilities为--info ==========
        // 旧版v4l2-ctl无--query-capabilities，用--info获取Capabilities
        QString devInfo = execute_command("v4l2-ctl",
                                         QStringList() << "--device=" + devicePath << "--info",
                                         5000); // 延长超时适配USBIPD

        bool hasVideoCapture = false;
        int deviceCapsStart = devInfo.indexOf("Device Caps", Qt::CaseInsensitive);
        if (deviceCapsStart != -1) {
            // 从Device Caps位置开始，向后查找Video Capture（只在Device Caps块内匹配）
            int videoCapturePos = devInfo.indexOf("Video Capture", deviceCapsStart, Qt::CaseInsensitive);
            hasVideoCapture = (videoCapturePos != -1);
        }

        if (!hasVideoCapture) {
            emit this->add_Logs(QString("[DeviceManageWidget]skip non-capture device: %1").arg(devicePath));
            continue;
        }

        // 解析设备名称（兼容旧版v4l2-ctl输出格式）
        QString cardName = "Unknown USBIPD Camera";
        if (devInfo.contains("Card type"))
        {
            int cardTypeIdx = devInfo.indexOf("Card type");
            int start = cardTypeIdx + 9; // "Card type: " 长度9
            int end = devInfo.indexOf('\n', start);
            if (end == -1) end = devInfo.length();
            if (start < devInfo.length()) {
                cardName = devInfo.mid(start, end - start).trimmed();
                cardName = cardName.simplified();
            }
        }

        // 构造显示文本（标注USBIPD+有效设备）
        QString displayText = QString("%1 - %2 (USBIPD, Video Capture)").arg(devicePath).arg(cardName);

        // 创建树形项并存储纯路径
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->videoDeviceTree);
        item->setText(0, displayText);
        item->setText(1, "USBIPD Video Device");
        item->setText(2, "to test");
        item->setData(0, Qt::UserRole, devicePath);

        //emit this->add_Logs(QString("[DeviceManageWidget]find valid USBIPD video device: %1").arg(displayText));
    }

    int validDeviceCount = ui->videoDeviceTree->topLevelItemCount();
    emit this->add_Logs(QString("[DeviceManageWidget]total find valid USBIPD video devices: %1").arg(validDeviceCount));
}

void DeviceManageWidget::scan_audio_devices()
{
    emit this->add_Logs("[DeviceManageWidget]start scan audio device");

    // 使用 arecord -l 列出录音设备
    QString output = execute_command("arecord", QStringList() << "-l");

    if (output.isEmpty() || output.contains("no soundcards found"))
    {
        emit this->add_Logs("[DeviceManageWidget]no audio input device was found. Adding the default device");

        // 添加默认的PulseAudio设备
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->audioDeviceTree);
        item->setText(0, "default (PulseAudio default device)");
        item->setText(1, "PulseAudio");
        item->setText(2, "no test");
        item->setData(0, Qt::UserRole, "default");

        emit this->add_Logs("[DeviceManageWidget]add default adudio device: default (PulseAudio)");
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

            //emit this->add_Logs(QString("[DeviceManageWidget]find audio device: %1 - %2").arg(hwDevice).arg(deviceName.isEmpty() ? cardName : deviceName));
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

        //emit this->add_Logs("[DeviceManageWidget]add default audio device: default");
    }

    emit this->add_Logs(QString("[DeviceManageWidget]total find %1 audio devices").arg(ui->audioDeviceTree->topLevelItemCount()));
}

void DeviceManageWidget::on_scan_button_clicked()
{
    emit this->add_Logs("[DeviceManageWidget]start scan device");

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

    // 收集设备列表
    QStringList videoDeviceList;
    QStringList audioDeviceList;
    QStringList videoDevicePathList;
    QStringList audioDeviceIdList;

    // 收集视频设备
    for (int i = 0; i < ui->videoDeviceTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = ui->videoDeviceTree->topLevelItem(i);
        QString devicePath = item->data(0, Qt::UserRole).toString();
        QString displayText = item->text(0); // 包含设备名称的完整文本
        videoDeviceList.append(displayText);
        videoDevicePathList.append(devicePath);

    }

    // 收集音频设备
    for (int i = 0; i < ui->audioDeviceTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = ui->audioDeviceTree->topLevelItem(i);
        QString deviceId = item->data(0, Qt::UserRole).toString();
        QString displayText = item->text(0); // 包含设备名称的完整文本
        audioDeviceList.append(displayText);
        audioDeviceIdList.append(deviceId);
    }

    emit this->set_scaned_devices(videoDeviceList,audioDeviceList,videoDevicePathList,audioDeviceIdList);
    // 完成
    emit this->add_Logs("[DeviceManageWidget]scan device over");

}

void DeviceManageWidget::on_video_device_double_clicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    // 获取设备路径
    QString devicePath = item->data(0, Qt::UserRole).toString();

    this->add_Logs(QString("[DeviceManageWidget]view details of video equipment: %1").arg(devicePath));

    // 清空详细信息
    ui->videoDetailTable->setRowCount(0);
    ui->videoFormatsList->clear();

    // 获取设备详细信息
    QString deviceInfo = this->execute_command("v4l2-ctl",
                                         QStringList() << "--device=" + devicePath << "--all");

    if (deviceInfo.isEmpty())
    {
        this->add_Logs("[DeviceManageWidget]fail to access device details");
        return;
    }

    // 解析并显示设备信息
    QStringList lines = deviceInfo.split('\n');
    int row = 0;

    for (const QString &line : lines)
    {
        if (line.contains(':'))
        {
            QStringList parts = line.split(':');
            if (parts.size() >= 2)
            {
                ui->videoDetailTable->insertRow(row);
                ui->videoDetailTable->setItem(row, 0,
                                              new QTableWidgetItem(parts[0].trimmed()));
                ui->videoDetailTable->setItem(row, 1,
                                              new QTableWidgetItem(parts[1].trimmed()));
                row++;
            }
        }

        // 提取支持的格式
        if (line.contains("Pixel Format"))
        {
            ui->videoFormatsList->addItem(line.trimmed());
        }
    }

}

void DeviceManageWidget::on_audio_device_double_clicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    // 获取设备标识
    QString deviceId = item->data(0, Qt::UserRole).toString();

    this->add_Logs(QString("[DeviceManageWidget]view details of audio devices: %1").arg(deviceId));

    // 清空详细信息
    ui->audioDetailTable->setRowCount(0);
    ui->audioCapabilitiesList->clear();

    // 添加基本信息
    ui->audioDetailTable->insertRow(0);
    ui->audioDetailTable->setItem(0, 0, new QTableWidgetItem("设备名称"));
    ui->audioDetailTable->setItem(0, 1, new QTableWidgetItem(item->text(0)));

    ui->audioDetailTable->insertRow(1);
    ui->audioDetailTable->setItem(1, 0, new QTableWidgetItem("类型"));
    ui->audioDetailTable->setItem(1, 1, new QTableWidgetItem(item->text(1)));

    ui->audioDetailTable->insertRow(2);
    ui->audioDetailTable->setItem(2, 0, new QTableWidgetItem("设备标识"));
    ui->audioDetailTable->setItem(2, 1, new QTableWidgetItem(deviceId));


    // 获取真实的设备能力
    QString hwParams = execute_command("arecord",
        QStringList() << "-D" << deviceId << "--dump-hw-params");

    if (!hwParams.isEmpty()) {
        // 解析输出
        QStringList lines = hwParams.split('\n');
        for (const QString &line : lines) {
            if (line.contains("RATE:") ||
                line.contains("FORMAT:") ||
                line.contains("CHANNELS:")) {
                ui->audioCapabilitiesList->addItem(line.trimmed());
            }
        }
    } else {
        // 如果获取失败，使用默认值
        ui->audioCapabilitiesList->addItem("采样率: 44100, 48000 Hz");
        ui->audioCapabilitiesList->addItem("格式: S16_LE, S32_LE");
        ui->audioCapabilitiesList->addItem("声道: 1 (单声道), 2 (立体声)");
    }


}

void DeviceManageWidget::on_test_video_button_clicked()
{
    if(this->selected_video_device.isEmpty()){
        this->add_Logs("[DeviceManageWidget]no video device selected");
        return;
    }
    QString devicePath = this->selected_video_device;
    this->add_Logs(QString("[DeviceManageWidget]start test USBIPD video device:%1").arg(devicePath));

    // 1. 临时提权（保留）
    QProcess chmodProcess;
    chmodProcess.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    chmodProcess.start("sudo", QStringList() << "chmod" << "666" << devicePath);
    chmodProcess.waitForFinished(5000);
    if (chmodProcess.exitCode() != 0) {
        this->add_Logs("[DeviceManageWidget][warning] chmod failed, video test may fail");
    }

    // 2. 检查FFmpeg（保留）
    QProcess whichProcess;
    whichProcess.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    whichProcess.start("which", QStringList() << "ffmpeg");
    whichProcess.waitForFinished(1000);
    QString ffmpegPath = whichProcess.readAllStandardOutput().trimmed();
    QFileInfo ffmpegInfo(ffmpegPath);
    if (ffmpegPath.isEmpty() || !ffmpegInfo.exists() || !ffmpegInfo.isExecutable()) {
        this->add_Logs(QString("[DeviceManageWidget][error] ffmpeg path invalid: %1").arg(ffmpegPath));
        return;
    }

    // ========== 核心修改：移除input_timeout，适配旧版FFmpeg ==========
    auto runFFmpegTest = [&]() -> bool {
        QProcess ffmpegProcess;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("LD_LIBRARY_PATH", "/usr/lib/x86_64-linux-gnu");
        ffmpegProcess.setProcessEnvironment(env);

        QStringList ffmpegArgs;
        ffmpegArgs << "-hide_banner"
                   << "-loglevel" << "error" // 只输出错误，减少日志
                   // 保留适配参数，移除input_timeout
                   << "-f" << "v4l2"
                   << "-input_format" << "mjpeg" // 通用MJPEG格式
                   << "-video_size" << "640x480" // 兼容分辨率
                   << "-framerate" << "10" // 降低帧率
                   << "-thread_queue_size" << "512" // 减小队列
                   << "-i" << devicePath
                   << "-t" << "2" // 测试时长2秒
                   << "-f" << "null"
                   << "-";

        // USBIPD设备就绪延迟（1.5秒）
        QThread::msleep(1500);
        ffmpegProcess.start(ffmpegPath, ffmpegArgs);

        // 等待启动
        if (!ffmpegProcess.waitForStarted(5000)) {
            this->add_Logs(QString("[DeviceManageWidget][error] ffmpeg start failed: %1").arg(ffmpegProcess.errorString()));
            return false;
        }

        // 用QProcess控制超时（8秒，替代input_timeout）
        bool isFinished = ffmpegProcess.waitForFinished(8000);
        if (!isFinished) {
            ffmpegProcess.kill();
            this->add_Logs("[DeviceManageWidget][warning] FFmpeg test timeout (8s)");
            return false;
        }

        // 解析结果
        QString stdErr = ffmpegProcess.readAllStandardError().trimmed();
        int exitCode = ffmpegProcess.exitCode();
        // 旧版FFmpeg成功时exitCode=0，或输出含frame=（表示采集到帧）
        bool success = (exitCode == 0) || stdErr.contains("frame=", Qt::CaseInsensitive);
        if (success) {
            this->add_Logs("[DeviceManageWidget][debug] FFmpeg test success: " + stdErr.left(200));
        } else {
            this->add_Logs("[DeviceManageWidget][debug] FFmpeg test failed, exit code: " + QString::number(exitCode) + ", error: " + stdErr.left(200));
        }
        return success;
    };

    // 执行测试（1次重试）
    bool testSuccess = runFFmpegTest();
    if (!testSuccess) {
        this->add_Logs("[DeviceManageWidget][warning] Retry FFmpeg test...");
        testSuccess = runFFmpegTest(); // 重试1次
    }

    // 结果处理+设备树更新
    auto updateTreeStatus = [&](bool isSuccess, const QString &statusMsg) {
        QTreeWidget *videoTree = ui->videoDeviceTree;
        for (int i = 0; i < videoTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = videoTree->topLevelItem(i);
            QString itemDevicePath = item->data(0, Qt::UserRole).toString();
            if (itemDevicePath == devicePath) {
                item->setText(2, QString("%1 (USBIPD)").arg(statusMsg));
                item->setIcon(2, isSuccess ? QIcon::fromTheme("dialog-ok") : QIcon::fromTheme("dialog-error"));
                videoTree->update();
                break;
            }
        }
    };

    if (testSuccess) {
        this->add_Logs("[DeviceManageWidget][success] USBIPD video device test passed");
        updateTreeStatus(true, "normal work");
    } else {
        this->add_Logs("[DeviceManageWidget][failure] USBIPD video test failed (8s x2)");
        updateTreeStatus(false, "test failed");
        // 适配旧版FFmpeg的排查提示
        this->add_Logs("[DeviceManageWidget][tip] Troubleshooting steps: 1. Run `ffmpeg -version` to check the version (it is recommended to upgrade to 5.0+). 2. Run `v4l2-ctl --list-formats-ext /dev/video0` to replace the input_format. 3. Close the processes that are using the camera.");
    }
}

void DeviceManageWidget::on_test_audio_button_clicked()
{
    // 1. 检查是否选中音频设备
    if (this->selected_audio_device.isEmpty()) {
        this->add_Logs("[DeviceManageWidget]no audio device selected");
        return;
    }
    QString devicePath = this->selected_audio_device;
    this->add_Logs(QString("[DeviceManageWidget]start test audio device:%1").arg(devicePath));

    // 2. 检查arecord是否安装
    QProcess whichProcess;
    whichProcess.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    whichProcess.start("which", QStringList() << "arecord");
    whichProcess.waitForFinished(1000);
    QString arecordPath = whichProcess.readAllStandardOutput().trimmed();

    if (arecordPath.isEmpty()) {
        this->add_Logs("[DeviceManageWidget][error] arecord not found! Install: sudo apt install alsa-utils");
        return;
    }

    // 3. 检测PulseAudio服务状态
    QString pulseaudioOutput = this->execute_command("pulseaudio", QStringList() << "--check");
    QProcess pulseProcess;
    pulseProcess.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    pulseProcess.start("pulseaudio", QStringList() << "--check");
    pulseProcess.waitForFinished(1000);

    if (pulseaudioOutput.isEmpty() && pulseProcess.exitCode() != 0) {
        this->add_Logs("[DeviceManageWidget][error] PulseAudio service not running! Execute: pulseaudio --start");
        this->add_Logs("[DeviceManageWidget][tip] WSL environment sudo：sudo apt install pulseaudio && pulseaudio --start");
        return;
    }

    // 4. 构造arecord测试参数
    QProcess audioProcess;
    audioProcess.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    QStringList arecordArgs;
    arecordArgs << "-f" << "cd"
                << "-d" << "3"
                << "-D" << devicePath
                << "/dev/null";

    // 5. 启动测试进程
    audioProcess.start(arecordPath, arecordArgs);

    // 6. 10秒超时等待
    const int AUDIO_TEST_TIMEOUT = 10000;
    bool isFinished = audioProcess.waitForFinished(AUDIO_TEST_TIMEOUT);

    // ========== 内联设备树更新逻辑 - 第一步：定义通用更新逻辑 ==========
    auto updateTreeStatus = [&](bool isSuccess, const QString &statusMsg) {
        // 【关键适配点1】替换为你实际的音频设备树控件名（比如ui->deviceTree）
        QTreeWidget *audioTree = ui->audioDeviceTree;
        // 遍历设备树匹配选中的音频设备
        for (int i = 0; i < audioTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = audioTree->topLevelItem(i);
            // 【关键适配点2】替换为你存储设备路径的UserRole（比如Qt::UserRole + 1）
            QString itemDevicePath = item->data(0, Qt::UserRole).toString();

            if (itemDevicePath == devicePath) {
                // 更新状态列（【关键适配点3】替换为状态列索引，比如1）
                item->setText(2, QString("%1 (Audio)").arg(statusMsg));
                // 设置状态图标
                item->setIcon(2, isSuccess ? QIcon::fromTheme("dialog-ok") : QIcon::fromTheme("dialog-error"));
                // 刷新UI
                audioTree->update();
                break;
            }
        }
    };

    // 7. 处理超时情况（直接调用内联更新逻辑）
    if (!isFinished) {
        audioProcess.kill();
        this->add_Logs(QString("[DeviceManageWidget][failure] audio test timeout (%1s)").arg(AUDIO_TEST_TIMEOUT / 1000));
        this->add_Logs("[DeviceManageWidget][tip] reason：1.PulseAudio setting fail 2.WSL audio connect fail 3.all audio hw cant use");
        // 内联更新：超时失败
        updateTreeStatus(false, "test timeout");
        return;
    }

    // 8. 解析正常退出结果
    int exitCode = audioProcess.exitCode();
    QString stdOut = audioProcess.readAllStandardOutput().trimmed();
    QString stdErr = audioProcess.readAllStandardError().trimmed();

    this->add_Logs(QString("[DeviceManageWidget][debug] arecord exit code: %1").arg(exitCode));
    if (!stdErr.isEmpty()) {
        this->add_Logs(QString("[DeviceManageWidget][debug] arecord error info: %1").arg(stdErr.left(300)));
    }

    // 9. 判断测试结果并内联更新设备树
    bool testSuccess = (exitCode == 0);
    QString statusMsg = testSuccess ? "normal work" : "test failed";

    // 内联更新：测试结果
    updateTreeStatus(testSuccess, statusMsg);

    // 10. 输出最终日志
    if (testSuccess) {
        this->add_Logs("[DeviceManageWidget][success] audio device test passed");
    } else {
        this->add_Logs("[DeviceManageWidget][failure] audio device test failed");
        if (stdErr.contains("No such device", Qt::CaseInsensitive)) {
            this->add_Logs("[DeviceManageWidget][tip] audio device is not present. Check the WSL audio bridging configuration.");
        } else if (stdErr.contains("Access denied", Qt::CaseInsensitive)) {
            this->add_Logs("[DeviceManageWidget][tip] insufficient permissions for audio equipment, execution failed.：sudo chmod 666 /dev/snd/*");
        }
    }
}
