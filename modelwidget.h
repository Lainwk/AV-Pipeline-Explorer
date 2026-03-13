#ifndef MODELWIDGET_H
#define MODELWIDGET_H

#include <QObject>
#include <QWidget>
#include <QProcess>

class ModelWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ModelWidget(QWidget *parent = nullptr);

signals:
    void add_Logs(const QString &message);

protected:
    // 当前选中的设备
    QString selected_video_device;
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

        // 同时读取stdout和stderr（FFmpeg主要输出到stderr）
        QString stdoutOutput = process.readAllStandardOutput().trimmed();
        QString stderrOutput = process.readAllStandardError().trimmed();
        QString allOutput = stdoutOutput + "\n" + stderrOutput;

        // 输出详细执行日志（便于排查）
        if (!allOutput.isEmpty()) {
            // emit add_Logs(QString("[Command][debug] command output:\n%1").arg(allOutput.left(500))); // 截断避免日志过长
        }
        emit add_Logs(QString("[Command][debug] command exit code: %1").arg(process.exitCode()));

        return allOutput;
    };

public slots:
    void set_selected_device(const QString &videoDevice,const QString &audioDevice);

};

#endif // MODELWIDGET_H
