#include "v4l2recordthread.h"

V4L2RecordThread::V4L2RecordThread()
{

}

V4L2RecordThread::~V4L2RecordThread()
{
    // 安全停止线程
    stopRecording();
    if (isRunning()) {
        quit();
        wait(1000); // 等待最多1秒
        if (isRunning()) {
            terminate(); // 强制终止
            wait();
        }
    }

    // 确保资源释放
    freeFFmpegResource();
}

void V4L2RecordThread::run()
{
    while (isRuning) {
        // 检查是否正在录制
        if (!isRuning || isPause) {
            QThread::msleep(10); // 短暂睡眠避免忙等待
            continue;
        }

        FrameData frameData;
        bool hasFrame = false;

        // 从队列获取帧数据
        {
            QMutexLocker locker(&queueMutex);
            if (frameQueue.isEmpty()) {
                // 队列为空，等待最多100毫秒
                queueCondition.wait(&queueMutex, 100);
            }

            if (!frameQueue.isEmpty()) {
                frameData = frameQueue.dequeue();
                hasFrame = true;
            }
        }

        // 处理帧数据
        if (hasFrame) {
            if (!encodeAndWriteFrame(frameData)) {
                this->addLocalLogs("[V4L2RecordThread][Error] encode || write frame fail");
                stopRecording();
            }

//            // 每编码10帧统计信息
//            if (encodedFrameCount % 10 == 0) {
//                QFileInfo fileInfo(this->fileOutputPath);
//                this->addLocalLogs(QString("[V4L2RecordThread][Logs] current file size:%1").arg(fileInfo.size()));
//            }
        }

        // 检查是否需要停止
        if (!isRecording && frameQueue.isEmpty()) {
            break;
        }
    }

}

const QAtomicInteger<bool> &V4L2RecordThread::getIsRecording() const
{
    return isRecording;
}

bool V4L2RecordThread::startRecording(const QString &filePath, const V4L2Params &newParams, EncodeFormat format)
{
    this->params = newParams;
    if(format == ENCODE_H264){
        this->fileOutputPath = filePath + "/" + this->getCurrentTime() + ".h264";
    } else if(format == ENCODE_H265){
        this->fileOutputPath = filePath + "/" + this->getCurrentTime() + ".h265";
    } else {
        this->addLocalLogs("[V4L2RecordThread][Error] invaild encode format");
        return false;
    }

    if(this->fileOutputPath.isEmpty()){
        return false;
    }

    //clear queue
    {
        QMutexLocker locker(&this->queueMutex);
        this->frameQueue.clear();
    }

    // init ffmpeg resource
    if (!this->initFFmpegResource()) {
        return false;
    }

    // start thread
    if(!this->isRunning()){
        this->isRuning = true;
        this->start();
    }

    // start record
    this->isRecording = true;
    this->isPause = false;
    this->isStop = false;
    this->encodedFrameCount = 0;
    this->startTime = QDateTime::currentMSecsSinceEpoch() * 1000;

    this->addLocalLogs("[V4L2RecordThread][Success] start recording");

    return true;

}

bool V4L2RecordThread::stopRecording()
{
    this->isRecording = false;
    this->queueCondition.wakeAll();

    if(!this->isRuning){
        return false;
    }  

    for (int i = 0; i < 20 && !this->frameQueue.isEmpty(); ++i) {
        QThread::msleep(100);
    }

    this->writeFileTrailer();
    this->freeFFmpegResource();

    qint64 duration = (QDateTime::currentMSecsSinceEpoch() * 1000 - startTime) / 1000; // 毫秒

    this->addLocalLogs(QString("[V4L2RecordThread][Success] stop recording success"));
    this->addLocalLogs(QString("---- file:%1").arg(this->fileOutputPath));
    this->addLocalLogs(QString("---- duration:%1").arg(duration));
    this->addLocalLogs(QString("---- frameCounts:%1").arg(this->encodedFrameCount));

    this->isRuning = false;
    this->encodedFrameCount = 0;

    return true;
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
        codec = avcodec_find_encoder_by_name("libx264");
        break;
    case ENCODE_H265:
        codec = avcodec_find_encoder_by_name("libx265");
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

    if (!yuyvFrame || !yuvFrame || !packet) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg allocate frame&packet fail");
        return false;
    }

    // 设置YUYV帧参数（输入格式）
    yuyvFrame->width = this->params.width;
    yuyvFrame->height = this->params.height;
    yuyvFrame->format = AV_PIX_FMT_YUYV422; // V4L2的YUYV对应FFmpeg的YUYV422

    // 为yuyvFrame分配缓冲区
    ret = av_frame_get_buffer(yuyvFrame, 32);
    if (ret < 0) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg allocate YUYV buffer fail");
        return false;
    }


    // 设置YUV帧参数（输出格式，用于编码）
    yuvFrame->width = this->params.width;
    yuvFrame->height = this->params.height;
    yuvFrame->format = this->codecContext->pix_fmt;

    ret = av_frame_get_buffer(yuvFrame, 32);
    if (ret < 0) {
        emit this->addLocalLogs("[V4L2RecordThread][Error] init ffmpeg allocate YUV buffer fail");
        return false;
    }

    AVPixelFormat inputPixFmt = v4l2ToFfmpegFormat(this->params.pixFmt);

    // 9. 创建图像转换上下文（YUYV -> YUV420P）
    swsContext = sws_getContext(
        this->params.width, this->params.height, inputPixFmt,  // 输入
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

void V4L2RecordThread::writeFileTrailer()
{
    if(this->formatContext){
        // refresh code buffer
        avcodec_send_frame(this->codecContext,nullptr);
        // 分配一个AVPacket结构体，用于存储编码后的数据包
        AVPacket *pkt = av_packet_alloc();
        // 检查分配是否成功
        if (pkt) {
            // 循环从编码器获取编码后的数据包
            while (true) {
                // 从编码器上下文接收编码完成的数据包
                int ret = avcodec_receive_packet(this->codecContext, pkt);

                // 检查返回值
                if (ret == AVERROR_EOF) {
                    // 编码器已刷新所有数据，没有更多包了，退出循环
                    break;
                } else if (ret >= 0) {
                    // 成功接收到一个有效的数据包

                    // 设置数据包所属的流索引（这里指定为视频流）
                    pkt->stream_index = this->videoStream->index;

                    // 将数据包的时间戳从编码器时间基转换为输出流时间基
                    av_packet_rescale_ts(pkt, this->codecContext->time_base, this->videoStream->time_base);

                    // 将数据包写入输出文件（自动交错排序）
                    av_interleaved_write_frame(this->formatContext, pkt);

                    // 释放数据包内部的资源，以便重用这个AVPacket结构
                    av_packet_unref(pkt);
                }
                // 注意：当 ret == AVERROR(EAGAIN) 时，表示编码器需要更多输入帧
                // 此时应继续外部循环，向编码器发送更多原始帧
            }
            // 释放AVPacket结构体本身占用的内存
            av_packet_free(&pkt);
        }
        // 写入文件尾
        av_write_trailer(this->formatContext);
    }
    this->addLocalLogs("[V4L2RecordThread][Success] writeFileTrailer success");
}

bool V4L2RecordThread::encodeAndWriteFrame(const FrameData &frameData)
{
    // 1. 确保YUYV帧可写
    int ret = av_frame_make_writable(yuyvFrame);
    if (ret < 0) {
        this->addLocalLogs("[V4L2RecordThread][Error] YUYV frame cannot be made writable");
        return false;
    }

    // 2. 计算正确的行字节数并复制数据
    int linesize = frameData.width * 2;
    if (frameData.data.size() < frameData.height * linesize) {
        this->addLocalLogs("[V4L2RecordThread][Error] Frame data size mismatch");
        return false;
    }

    // 将数据复制到yuyvFrame的缓冲区中
    for (int y = 0; y < frameData.height; ++y) {
        memcpy(yuyvFrame->data[0] + y * yuyvFrame->linesize[0],
               frameData.data.constData() + y * linesize,
               linesize);
    }

    ret = sws_scale(swsContext,
                    yuyvFrame->data, yuyvFrame->linesize, 0, frameData.height,
                    yuvFrame->data, yuvFrame->linesize);
    if (ret < 0) {
        this->addLocalLogs("[V4L2RecordThread][Error] Image format conversion failed");
        return false;
    }


    // 3. 设置时间戳
    yuvFrame->pts = encodedFrameCount;

    // 4. 编码帧
    ret = avcodec_send_frame(codecContext, yuvFrame);
    if (ret < 0) {
        qWarning() << "[V4L2RecordThread] 发送帧到编码器失败:" << ret;
        this->addLocalLogs("[V4L2RecordThread][Error] failed to send frames to the encoder");
        return false;
    }

    // 5. 接收编码后的包
    while (ret >= 0) {
        ret = avcodec_receive_packet(codecContext, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            qWarning() << "[V4L2RecordThread] 从编码器接收包失败:" << ret;
            this->addLocalLogs("[V4L2RecordThread][Error] failure in receiving packets from the encoder");
            return false;
        }

        // 设置流索引和时间戳
        packet->stream_index = videoStream->index;
        av_packet_rescale_ts(packet, codecContext->time_base, videoStream->time_base);

        // 6. 写入文件
        ret = av_interleaved_write_frame(formatContext, packet);
        av_packet_unref(packet);

        if (ret < 0) {
            qWarning() << "[V4L2RecordThread] 写入帧失败:" << ret;
            this->addLocalLogs("[V4L2RecordThread][Error] frame writing fail");
            return false;
        }
    }

    encodedFrameCount++;
    return true;

}

QString V4L2RecordThread::getCurrentTime()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QString timeString = currentTime.toString("yyyy-MM-dd_HH:mm:ss");
    return timeString;
}

AVPixelFormat V4L2RecordThread::v4l2ToFfmpegFormat(uint32_t v4l2PixFmt)
{
    switch (v4l2PixFmt) {
    case V4L2_PIX_FMT_YUYV:  // 'YUYV'
        return AV_PIX_FMT_YUYV422;
    case V4L2_PIX_FMT_MJPEG: // 'MJPG'
        return AV_PIX_FMT_YUVJ420P;
    case V4L2_PIX_FMT_YUV420: // 'YU12'
        return AV_PIX_FMT_YUV420P;
    case V4L2_PIX_FMT_NV12:   // 'NV12'
        return AV_PIX_FMT_NV12;
    case V4L2_PIX_FMT_RGB24:  // 'RGB3'
        return AV_PIX_FMT_RGB24;
    default:
        qWarning() << "[V4L2RecordThread] 未知的V4L2像素格式:"
                  << QString("%1").arg(v4l2PixFmt, 0, 16);
    return AV_PIX_FMT_NONE;
    }
}

const QAtomicInteger<bool> &V4L2RecordThread::getIsPause() const
{
    return isPause;
}

void V4L2RecordThread::setIsPause(const QAtomicInteger<bool> &newIsPause)
{
    isPause = newIsPause;
}

void V4L2RecordThread::receivedFrame(const uchar *data,int size, int width, int height)
{
    if (!isRuning) {
        return;
    }

    if(!data || width != this->params.width || height != this->params.height){
        return;
    }

    AVPixelFormat ffmpegPixFmt = v4l2ToFfmpegFormat(this->params.pixFmt);
    if (ffmpegPixFmt == AV_PIX_FMT_NONE) {
        qWarning() << "[V4L2RecordThread] 不支持的像素格式:" << this->params.pixFmt;
        return;
    }

    QMutexLocker locker(&queueMutex);
    if (frameQueue.size() >= MAX_QUEUE_SIZE) {
        // 队列已满，丢弃最旧的一帧
        frameQueue.dequeue();
        qWarning() << "[V4L2RecordThread] 队列已满，丢弃一帧，当前队列大小:" << frameQueue.size();
    }

    FrameData frameData;
    frameData.data = QByteArray(reinterpret_cast<const char*>(data), size);
    frameData.width = width;
    frameData.height = height;
    frameData.pixFmt = ffmpegPixFmt;
    frameData.timestamp = QDateTime::currentMSecsSinceEpoch() * 1000; // 微秒

    frameQueue.enqueue(frameData);
    queueCondition.wakeOne();

}

void V4L2RecordThread::receivedStartThreadSiganl(const QString &filePath, const V4L2Params &newParams, EncodeFormat format)
{
    this->startRecording(filePath,newParams,format);
}

void V4L2RecordThread::receivedStopThreadSignal()
{
    this->stopRecording();
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
