#ifndef V4L2PREVIEWTHREAD_H
#define V4L2PREVIEWTHREAD_H

#include <QObject>
#include <QThread>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include <linux/videodev2.h>
#include <fcntl.h>      // open/close
#include <unistd.h>     // close
#include <errno.h>      // errno
#include <string.h>     // strerror
#include <sys/mman.h>   // mmap
#include <sys/ioctl.h>
#include <modelwidget.h>
#include <sys/select.h>
#include <QString>
#include <QDebug>

class V4L2PreviewThread : public QThread
{
    Q_OBJECT
public:
    V4L2PreviewThread();
    ~V4L2PreviewThread() override;

    void stop_Preview();

    void setParams(const V4L2Params &newParams);

    void setV4l2Fd(int newV4l2Fd);

protected:
    void run() override;

private:
    // V4L2缓冲区结构体（存储映射后的内存地址和长度）
    struct Buffer {
        void *start;
        size_t length;
    };

    int v4l2Fd = -1;                // V4L2设备文件描述符
    bool isRunning = false;         // 线程运行标志
    QMutex mutex;                   // 线程安全锁
    QWaitCondition cond;            // 等待条件
    Buffer *buffers = nullptr;      // 缓冲区数组
    int bufferCount = 0;            // 缓冲区数量
    V4L2Params params;              // 视频采集参数

    bool init_V4L2_Buffers();
    void release_V4L2_Buffers();
    // 设置V4L2采集格式和帧率
    bool set_V4L2_Format_And_Fps();
    // 启动V4L2流采集
    bool start_V4L2_Stream();
    // 停止V4L2流采集
    void stop_V4L2_Stream();


signals:
    // 预览图像发送给UI线程
    void previewImageReady(const uchar *Data, int size,int width, int height);


    // add logs
    void add_Logs(const QString &logmessage);

    //params warning
    void paramsWarning(const QString &title,const QString &message);
};

#endif // V4L2PREVIEWTHREAD_H
