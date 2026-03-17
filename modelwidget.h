#ifndef MODELWIDGET_H
#define MODELWIDGET_H

#include <QObject>
#include <QWidget>
#include <QProcess>
#include <linux/videodev2.h>
#include <fcntl.h>      // open/close
#include <unistd.h>     // close
#include <errno.h>      // errno
#include <string.h>     // strerror
#include <QFrame>
// V4L2参数结构体
struct V4L2Params {
    int width = 640;               // 默认宽度
    int height = 480;              // 默认高度
    int fps = 30;                  // 默认帧率
    uint32_t pixFmt = V4L2_PIX_FMT_YUYV; // 默认像素格式
};

struct selectedDeviceV4L2Params {
    QString selectedVideoDevice;          // 设备路径（/dev/videoX）
    QList<QSize> supportRes;              // 支持的分辨率列表
    QList<int> supportFps;                // 对应分辨率的帧率列表
    V4L2Params defaultParams;             // 默认采集参数（启动时用）
    uint32_t supportPixFmt = V4L2_PIX_FMT_YUYV; // 支持的像素格式（优先YUYV）
};

// 新增：注册自定义类型，让Qt识别
Q_DECLARE_METATYPE(selectedDeviceV4L2Params);
// 若传递列表，额外注册列表类型
Q_DECLARE_METATYPE(QList<selectedDeviceV4L2Params>);

class ModelWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ModelWidget(QWidget *parent = nullptr);

public slots:
    void set_select_device(const selectedDeviceV4L2Params &newCurrentVideoDeviceParams, const QString &audioDevice);

signals:
    void add_Logs(const QString &message);

protected:
    // 当前选中的设备
    selectedDeviceV4L2Params currentVideoDeviceParams; // 新增：当前选中设备的V4L2参数
    QString selected_audio_device;

    virtual void init_connect() = 0;
    QString execute_command(const QString &command, const QStringList &arguments, int timeout = 10000)
    {
        QProcess process;
        // 继承系统环境变量（关键：解决FFmpeg路径/依赖问题）
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        process.setProcessEnvironment(env);

        // 合并命令+参数，用于日志排查
        QString cmdStr = command + " " + arguments.join(" ");
        emit add_Logs(QString("[Command][debug] execute command: %1").arg(cmdStr));

        process.start(command, arguments);

        // 延长默认超时到10秒，支持自定义超时
        bool finished = process.waitForFinished(timeout);
        if (!finished) {
            process.kill();
            emit add_Logs(QString("[Command][debug] command timeout (>=%1ms), kill process").arg(timeout));
            return QString();
        }

        // 同时读取stdout和stderr
        QString stdoutOutput = process.readAllStandardOutput().trimmed();
        QString stderrOutput = process.readAllStandardError().trimmed();
        QString allOutput = stdoutOutput + "\n" + stderrOutput;

        // 输出详细执行日志（便于排查）
        if (!allOutput.isEmpty()) {
            // emit add_Logs(QString("[Command][debug] command output:\n%1").arg(allOutput.left(500))); // 截断避免日志过长
        }
        emit add_Logs(QString("[Command][debug] command exit code: %1").arg(process.exitCode()));

        return allOutput;
    }


};
#endif // MODELWIDGET_H
