#include "v4l2recordthread.h"

V4L2RecordThread::V4L2RecordThread()
{

}

V4L2RecordThread::~V4L2RecordThread()
{

}

void V4L2RecordThread::run()
{

}

bool V4L2RecordThread::startRecording(const QString &filePath, const V4L2Params &newParams, EncodeFormat format)
{
    this->params = newParams;
    this->fileOutputPath = filePath + "/" + this->getCurrentTime();

    if(this->fileOutputPath.isEmpty()){
        return false;
    }
}

bool V4L2RecordThread::initFFmpegResource()
{
    // 1. 分配封装上下文
    int ret = avformat_alloc_output_context2(&this->formatContext,
                                             nullptr,
                                             nullptr,
                                             this->fileOutputPath.toUtf8().constData());
    if(ret < 0 || !this->formatContext) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg AVFormatContext fail");
        return false;
    }

    // 2. 查找编码器
    const AVCodec *codec = nullptr;
    switch(this->encodeFormat){
    case ENCODE_H264:
        codec = avcodec_find_decoder_by_name("libx264");
        break;
    case ENCODE_H265:
        codec = avcodec_find_decoder_by_name("libx265");
        break;
    default:
        codec = nullptr;
    }

    if(codec == nullptr){
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg AVCODEC fail");
        return false;
    }

    // 3. 创建编码器上下文
    this->codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg AVCodecContext fail");
        return false;
    }

    // 设置编码参数
    codecContext->width = this->params.width;
    codecContext->height = this->params.height;
    codecContext->time_base = (AVRational){1, this->params.fps};
    codecContext->framerate = (AVRational){this->params.fps, 1};
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    codecContext->gop_size = this->params.fps; // 关键帧间隔（1秒）
    codecContext->max_b_frames = 1; // B帧数量

    // 设置质量/码率参数
    if (this->formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        this->formatContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 4. 打开编码器
    AVDictionary *codecOptions = nullptr;
    av_dict_set(&codecOptions, "preset", "medium", 0); // 编码预设
    av_dict_set(&codecOptions, "crf", "23", 0); // 质量因子（0-51，越小质量越好）

    ret = avcodec_open2(codecContext, codec, &codecOptions);
    av_dict_free(&codecOptions);

    if (ret < 0) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg open encoder fail");
        return false;
    }

    // 5. 创建视频流
    this->videoStream = avformat_new_stream(formatContext, nullptr);
    if (!this->videoStream) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg creat VideoStream fail");
        return false;
    }

    this->videoStream->id = this->formatContext->nb_streams - 1;
    this->videoStream->time_base = this->codecContext->time_base;

    ret = avcodec_parameters_from_context(this->videoStream->codecpar, this->codecContext);
    if (ret < 0) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg copy encode params to stream fail");
        return false;
    }

    // 6. 打开输出文件
    if (!(this->formatContext->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&this->formatContext->pb, this->fileOutputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            emit this->addLocalLogs(QString("[V4L2RecordThread][Error] init ffmpeg open outputFile fail:%1")
                                    .arg(this->fileOutputPath));
            return false;
        }
    }

    // 7. 写入文件头
    ret = avformat_write_header(this->formatContext, nullptr);
    if (ret < 0) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg write file header fail");
        return false;
    }

    // 8. 分配帧和包
    yuyvFrame = av_frame_alloc();
    yuvFrame = av_frame_alloc();
    packet = av_packet_alloc();

    if (!yuyvFrame || !yuvFrame || packet) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg allocate frame&packet fail");
        return false;
    }

    // 设置YUYV帧参数（输入格式）
    yuyvFrame->width = this->params.width;
    yuyvFrame->height = this->params.height;
    yuyvFrame->format = AV_PIX_FMT_YUYV422; // V4L2的YUYV对应FFmpeg的YUYV422

    // 设置YUV帧参数（输出格式，用于编码）
    yuvFrame->width = this->params.width;
    yuvFrame->height = this->params.height;
    yuvFrame->format = this->codecContext->pix_fmt;

    ret = av_frame_get_buffer(yuvFrame, 32);
    if (ret < 0) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg allocate YUV buffer fail");
        return false;
    }

    // 9. 创建图像转换上下文（YUYV -> YUV420P）
    swsContext = sws_getContext(
        this->params.width, this->params.height, AV_PIX_FMT_YUYV422,  // 输入
        this->params.width, this->params.height, AV_PIX_FMT_YUV420P,  // 输出
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!this->swsContext) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg creat swsContext fail");
        return false;
    }

    emit this->addLocalLogs("[V4L2RecordThread][Success] init ffmpeg success");
    emit this->addLocalLogs(QString("---- resolution:%1 x %2")
                            .arg(this->params.width)
                            .arg(this->params.height));
    emit this->addLocalLogs(QString("---- fps:%1")
                            .arg(this->params.fps));
    emit this->addLocalLogs(QString("---- pixFmt:%1")
                            .arg(this->encodeFormat));

    return true;
}

bool V4L2RecordThread::freeFFmpegResource()
{
    // 释放图像转换上下文
    if (swsContext) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }

    // 释放帧
    if (yuyvFrame) {
        av_frame_free(&yuyvFrame);
    }

    if (yuvFrame) {
        av_frame_free(&yuvFrame);
    }

    // 释放包
    if (packet) {
        av_packet_free(&packet);
    }

    // 关闭编码器上下文
    if (codecContext) {
        avcodec_free_context(&codecContext);
    }

    // 关闭输出文件
    if (formatContext) {
        if (formatContext->pb && !(formatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&formatContext->pb);
        }
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }

    videoStream = nullptr;

    emit this->addLocalLogs("[V4L2RecordThread][Success] release ffmpeg resource success");
}

QString V4L2RecordThread::getCurrentTime()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QString timeString = currentTime.toString("yyyy-MM-dd_HH:mm:ss");
    return timeString;
}

const QAtomicInteger<bool> &V4L2RecordThread::getIsPause() const
{
    return isPause;
}

void V4L2RecordThread::setIsPause(const QAtomicInteger<bool> &newIsPause)
{
    isPause = newIsPause;
}

const QAtomicInteger<bool> &V4L2RecordThread::getIsStop() const
{
    return isStop;
}

void V4L2RecordThread::setIsStop(const QAtomicInteger<bool> &newIsStop)
{
    isStop = newIsStop;
}

const QAtomicInteger<bool> &V4L2RecordThread::getIsRuning() const
{
    return isRuning;
}

void V4L2RecordThread::setIsRuning(const QAtomicInteger<bool> &newIsRuning)
{
    isRuning = newIsRuning;
}
