#include "v4l2previewthread.h"

V4L2PreviewThread::V4L2PreviewThread()
{

}

V4L2PreviewThread::~V4L2PreviewThread()
{
    this->stop_Preview();
    wait();
    this->release_V4L2_Buffers();
}

void V4L2PreviewThread::stop_Preview()
{
    QMutexLocker locker(&mutex);
    this->isRunning = false;
    cond.wakeAll();
}

void V4L2PreviewThread::run()
{
    QMutexLocker locker(&mutex);
    if (v4l2Fd < 0) {
        emit this->add_Logs("[V4L2PreviewThread][Error] V4L2 device not opened");
        return;
    }
    isRunning = true;
    locker.unlock();

    //1.
    if(!this->set_V4L2_Format_And_Fps()){
        return;
    }

    //2.
    if(!this->init_V4L2_Buffers()){
        return;
    }

    //3.
    if(!this->start_V4L2_Stream()){
        this->release_V4L2_Buffers();
        return;
    }

    //4.read frame data to buffer
    emit this->add_Logs("[V4L2PreviewThread][Debug] Entering frame capture loop with timeout");

    int frameCount = 0;
    int timeoutCount = 0;
    const int MAX_TIMEOUT = 10; // 最多允许10次超时

    while(this->isRunning){
        // 使用select检测设备是否有数据可读（超时5秒）
        fd_set fds;
        struct timeval tv;
        FD_ZERO(&fds);
        FD_SET(this->v4l2Fd, &fds);
        tv.tv_sec = 5;  // 5秒超时
        tv.tv_usec = 0;

        int ret = select(this->v4l2Fd + 1, &fds, nullptr, nullptr, &tv);
        if (ret < 0) {
            emit this->add_Logs(QString("[V4L2PreviewThread][Error] select() failed: %1 (errno: %2)")
                              .arg(strerror(errno)).arg(errno));
            break;
        } else if (ret == 0) {
            // 超时
            timeoutCount++;
            emit this->add_Logs(QString("[V4L2PreviewThread][Warning] Timeout waiting for frame (count: %1)")
                              .arg(timeoutCount));
            if (timeoutCount >= MAX_TIMEOUT) {
                emit this->add_Logs("[V4L2PreviewThread][Error] Too many timeouts, stopping capture");
                break;
            }
            continue;
        }

        // 有数据可读，重置超时计数
        timeoutCount = 0;

        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        ret = ioctl(this->v4l2Fd, VIDIOC_DQBUF, &buf);
        if(ret < 0){
            emit this->add_Logs(QString("[V4L2PreviewThread][Error] Dequeue buffer failed: %1 (errno: %2)")
                              .arg(strerror(errno)).arg(errno));
            break;
        }

        // 成功获取帧
        frameCount++;
        if (frameCount == 1) {
            emit this->add_Logs("[V4L2PreviewThread][Success] First frame captured!");
        }
        if (frameCount % 30 == 0) {
            emit this->add_Logs(QString("[V4L2PreviewThread][Success] Captured frame #%1").arg(frameCount));
        }

        emit this->previewImageReady(static_cast<uchar*>(buffers[buf.index].start),
                this->params.width, this->params.height);

        // 将缓冲区重新入队
        if (ioctl(v4l2Fd, VIDIOC_QBUF, &buf) < 0) {
            emit this->add_Logs(QString("[V4L2PreviewThread][Error] Requeue buffer failed: %1 (errno: %2)")
                              .arg(strerror(errno)).arg(errno));
            break;
        }
    }

    //release
    this->stop_V4L2_Stream();
    this->release_V4L2_Buffers();
    emit this->add_Logs("[V4L2PreviewThread][Success] stop V4L2 thread run loop success");

}

void V4L2PreviewThread::setV4l2Fd(int newV4l2Fd)
{
    QMutexLocker locker(&this->mutex);
    v4l2Fd = newV4l2Fd;
}

void V4L2PreviewThread::setParams(const V4L2Params &newParams)
{
    QMutexLocker locker(&this->mutex);
    params = newParams;
}

bool V4L2PreviewThread::init_V4L2_Buffers()
{
    if (v4l2Fd < 0) {
        emit this->add_Logs("[V4L2PreviewThread][Error] V4L2 device not opened");
        return false;
    }

    // 1. 请求缓冲区
    struct v4l2_requestbuffers req = {0};
    req.count = 4; // 请求4个缓冲区
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(v4l2Fd, VIDIOC_REQBUFS, &req) < 0) {
        emit this->add_Logs(QString("[V4L2PreviewThread][Error] Request buffers failed: %1 (errno: %2)")
                          .arg(strerror(errno)).arg(errno));
        return false;
    }

    bufferCount = req.count;
    buffers = new Buffer[bufferCount];
    if (!buffers) {
        this->add_Logs("[V4L2PreviewThread][Error] Allocate buffers array failed");
        return false;
    }

    // 2. 映射每个缓冲区到用户空间
    for (int i = 0; i < bufferCount; ++i) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        //使用内存映射方式
        //零拷贝技术，将内核空间缓冲区直接映射到用户空间
        //效率最高，CPU占用最少

        if (ioctl(v4l2Fd, VIDIOC_QUERYBUF, &buf) < 0) {
            this->add_Logs(QString("[V4L2PreviewThread][Error] Query buffer %1 failed: %2 (errno: %3)")
                              .arg(i).arg(strerror(errno)).arg(errno));
            release_V4L2_Buffers();
            return false;
        }

        buffers[i].length = buf.length;
        buffers[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, v4l2Fd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED) {
            this->add_Logs(QString("[V4L2PreviewThread][Error] Mmap buffer %1 failed: %2 (errno: %3)")
                              .arg(i).arg(strerror(errno)).arg(errno));
            release_V4L2_Buffers();
            return false;
        }

        // 3. 将缓冲区放入队列
        if (ioctl(v4l2Fd, VIDIOC_QBUF, &buf) < 0) {
            this->add_Logs(QString("[V4L2PreviewThread][Error] Queue buffer %1 failed: %2 (errno: %3)")
                              .arg(i).arg(strerror(errno)).arg(errno));
            release_V4L2_Buffers();
            return false;
        }
    }

    emit this->add_Logs("[V4L2PreviewThread][Success] Set V4L2 buffers success");

    return true;
}

void V4L2PreviewThread::release_V4L2_Buffers()
{
    if (this->buffers) {
        for (int i = 0; i < bufferCount; ++i) {
            if (buffers[i].start != MAP_FAILED) {
                munmap(buffers[i].start, buffers[i].length);
            }
        }
        delete[] buffers;
        buffers = nullptr;
        bufferCount = 0;

        emit this->add_Logs("[V4L2PreviewThread][Success] release V4L2 buffers success");

    }
}

bool V4L2PreviewThread::set_V4L2_Format_And_Fps()
{
    if(this->v4l2Fd < 0){
        emit this->add_Logs("[V4L2PreviewThread][Error] V4L2 device not opened");
        return false;
    }

    emit this->add_Logs(QString("[V4L2PreviewThread][Debug] Setting format: %1x%2, FPS: %3, PixFmt: 0x%4")
                           .arg(params.width)
                           .arg(params.height)
                           .arg(params.fps)
                           .arg(params.pixFmt, 0, 16));
    //set image format
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = params.width;
    fmt.fmt.pix.height = params.height;
    fmt.fmt.pix.pixelformat = params.pixFmt;
    fmt.fmt.pix.field = V4L2_FIELD_NONE; // 无场（逐行）

    if (ioctl(v4l2Fd, VIDIOC_S_FMT, &fmt) < 0) {
        emit this->add_Logs(QString("[V4L2PreviewThread][Error]Set format failed: %1 (errno: %2)")
                          .arg(strerror(errno)).arg(errno));
        return false;
    }

    emit this->add_Logs(QString("[V4L2PreviewThread][Debug] Actual format: %1x%2, PixFmt: 0x%3")
                           .arg(fmt.fmt.pix.width)
                           .arg(fmt.fmt.pix.height)
                           .arg(fmt.fmt.pix.pixelformat, 0, 16));

    // 2. 设置帧率
    struct v4l2_streamparm streamparm = {0};
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = params.fps;

    if (ioctl(v4l2Fd, VIDIOC_S_PARM, &streamparm) < 0) {
        emit this->add_Logs(QString("[V4L2PreviewThread][Error] Set fps failed: %1 (errno: %2)")
                          .arg(strerror(errno)).arg(errno));
        return false;
    }

    emit this->add_Logs(QString("[V4L2PreviewThread][Debug] Actual FPS: %1/%2 = %3").arg(streamparm.parm.capture.timeperframe.denominator)
                        .arg(streamparm.parm.capture.timeperframe.numerator)
                        .arg(streamparm.parm.capture.timeperframe.denominator /
                             streamparm.parm.capture.timeperframe.numerator));
    emit this->add_Logs("[V4L2PreviewThread][Success] Set V4L2 format & fps success");

    return true;

}



bool V4L2PreviewThread::start_V4L2_Stream()
{
    //启动视频流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(v4l2Fd, VIDIOC_STREAMON, &type) < 0) {
         emit this->add_Logs(QString("[V4L2PreviewThread][Error] Start stream failed: %1 (errno: %2)")
                          .arg(strerror(errno)).arg(errno));
        return false;
    }
    emit this->add_Logs("[V4L2PreviewThread][Success] start V4L2 stream success");

    emit this->add_Logs("[V4L2PreviewThread][Debug] Triggering USBIPD device data stream...");

    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(v4l2Fd, &fds);
    tv.tv_sec = 2;  // 等待2秒让设备启动
    tv.tv_usec = 0;

    int ret = select(v4l2Fd + 1, &fds, nullptr, nullptr, &tv);
    if (ret > 0) {
        emit this->add_Logs("[V4L2PreviewThread][Success] Device ready, data stream active");
    } else if (ret == 0) {
        emit this->add_Logs("[V4L2PreviewThread][Warning] Device startup timeout, but will continue trying");
    } else {
        emit this->add_Logs(QString("[V4L2PreviewThread][Warning] Device check failed: %1").arg(strerror(errno)));
    }

    return true;
}


void V4L2PreviewThread::stop_V4L2_Stream()
{
    if (v4l2Fd < 0) return;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(v4l2Fd, VIDIOC_STREAMOFF, &type);
    emit this->add_Logs("[V4L2PreviewThread][Success] stop V4L2 stream success");

}
