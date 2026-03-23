#ifndef V4L2RECORDTHREAD_H
#define V4L2RECORDTHREAD_H

#include <QObject>
#include <QThread>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <QQueue>
#include <QPair>
#include <QByteArray>
#include <QAtomicInteger>
#include <linux/videodev2.h>
#include <QDebug>
#include <QDateTime>
#include "modelwidget.h"

//FFMPEG
extern "C"
 {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/opt.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

enum EncodeFormat {
        ENCODE_H264 = 0, ///< H.264
        ENCODE_H265  ///< H.265
};

struct FrameData
 {
        QByteArray data;        ///< 帧数据
        int width;              ///< 帧宽度
        int height;             ///< 帧高度
        AVPixelFormat pixFmt;   ///< FFmpeg像素格式（从V4L2转换而来）
        qint64 timestamp;       ///< 时间戳
};

class V4L2RecordThread : public QThread
{
    Q_OBJECT
public:
    V4L2RecordThread();
    ~V4L2RecordThread() override;

    bool startRecording(const QString &filePath,const V4L2Params &newParams, EncodeFormat format);

    const QAtomicInteger<bool> &getIsRuning() const;
    void setIsRuning(const QAtomicInteger<bool> &newIsRuning);

    const QAtomicInteger<bool> &getIsStop() const;
    void setIsStop(const QAtomicInteger<bool> &newIsStop);

    const QAtomicInteger<bool> &getIsPause() const;
    void setIsPause(const QAtomicInteger<bool> &newIsPause);

protected:
    void run() override;

private:
    //thread control
    int setEncodeFormat;
    QString fileOutputPath;
    QAtomicInteger<bool> isRuning;
    QAtomicInteger<bool> isStop;
    QAtomicInteger<bool> isPause;
    V4L2Params params;
    EncodeFormat encodeFormat = ENCODE_H264;

    //frame queue
    QQueue<FrameData> frameQueue;
    QMutex queueMutex;
    QWaitCondition queueCondition;
    const int MAX_QUEUE_SIZE = 30;

    //ffmpeg
    AVFormatContext *formatContext;  ///< 封装格式上下文
    AVCodecContext *codecContext;    ///< 编码器上下文
    AVStream *videoStream;           ///< 视频流
    SwsContext *swsContext;          ///< 图像缩放/转换上下文
    AVFrame *yuyvFrame;              ///< 输入的YUYV帧
    AVFrame *yuvFrame;               ///< 转换后的YUV帧（用于编码）
    AVPacket *packet;                ///< 编码后的数据
    bool ffmpegInilized;

    //statistics


    //func
    bool initFFmpegResource();
    bool freeFFmpegResource();

    //tool func
    QString getCurrentTime();

signals:
    void addLocalLogs(const QString &message);


};

#endif // V4L2RECORDTHREAD_H
