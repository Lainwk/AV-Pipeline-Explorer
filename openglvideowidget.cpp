#include "openglvideowidget.h"


OpenGLVideoWidget::OpenGLVideoWidget(QWidget *parent)
{
    // 设置OpenGL格式（适配WSL）
    QSurfaceFormat format;
    format.setVersion(4, 3);       // 兼容WSL Intel核显的OpenGL版本
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    setFormat(format);
}

OpenGLVideoWidget::~OpenGLVideoWidget()
{
    releaseGLResources();
    if (m_yuyvData) {
        delete[] m_yuyvData;
        m_yuyvData = nullptr;
    }
}

void OpenGLVideoWidget::initializeGL()
{
    emit this->add_Logs("[OpenGLVideoWidget][Operation] init openGL widget");
    initializeOpenGLFunctions(); // 初始化OpenGL函数
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 预览背景色（深灰）

    // 创建YUV纹理对象
    glGenTextures(1, &m_textureY);
    glGenTextures(1, &m_textureU);
    glGenTextures(1, &m_textureV);

    // 设置纹理参数（线性插值，适配缩放）
    glBindTexture(GL_TEXTURE_2D, m_textureY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, m_textureU);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, m_textureV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void OpenGLVideoWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h); // 设置视口大小（填满Widget）
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1); // 正交投影（适配2D渲染）
    glMatrixMode(GL_MODELVIEW);
}

void OpenGLVideoWidget::paintGL()
{
    QMutexLocker locker(&m_mutex); // 加锁防止数据读写冲突
   glClear(GL_COLOR_BUFFER_BIT);  // 清空缓冲区

   if (!m_hasFrame || !m_yuyvData) {
       // 无帧数据时，先用OpenGL绘制单色背景
       glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // all black
       glClear(GL_COLOR_BUFFER_BIT);

       // 然后使用QPainter绘制文字
       QPainter painter(this);
       painter.setRenderHint(QPainter::Antialiasing);
       painter.setPen(Qt::white);
       painter.setFont(QFont("Arial", 16));

       QString text = "无视频信号";
       QRect textRect = painter.fontMetrics().boundingRect(text);
       int x = (width() - textRect.width()) / 2;
       int y = (height() - textRect.height()) / 2;

       painter.drawText(x, y + textRect.height(), text);
       return;
   }

   // 更新YUV纹理
   updateYuvTextures();

   // 绘制纹理到屏幕（核心：YUYV转RGB的GPU渲染）
   glEnable(GL_TEXTURE_2D);
   glBegin(GL_QUADS);

   // 纹理坐标（0,0到1,1），顶点坐标适配Widget大小
   // 左上
   glTexCoord2f(0.0f, 0.0f);
   glVertex2f(0.0f, 0.0f);
   // 右上
   glTexCoord2f(1.0f, 0.0f);
   glVertex2f(width(), 0.0f);
   // 右下
   glTexCoord2f(1.0f, 1.0f);
   glVertex2f(width(), height());
   // 左下
   glTexCoord2f(0.0f, 1.0f);
   glVertex2f(0.0f, height());

   glEnd();
   glDisable(GL_TEXTURE_2D);
}

void OpenGLVideoWidget::updateYuvTextures()
{
    if (!m_yuyvData) return;

    // YUYV格式：Y0 U0 Y1 V0 | Y2 U2 Y3 V2 ... 拆分Y/U/V分量
    int ySize = m_frameWidth * m_frameHeight;
    int uvSize = ySize / 2;
    uchar *yData = new uchar[ySize];
    uchar *uData = new uchar[uvSize];
    uchar *vData = new uchar[uvSize];

    int yIdx = 0, uIdx = 0, vIdx = 0;
    for (int i = 0; i < ySize * 2; i += 4) {
        // 提取Y分量
        yData[yIdx++] = m_yuyvData[i];
        yData[yIdx++] = m_yuyvData[i + 2];
        // 提取U/V分量（每2个像素共享一组U/V）
        uData[uIdx++] = m_yuyvData[i + 1];
        vData[vIdx++] = m_yuyvData[i + 3];
    }

    // 更新Y纹理
    glBindTexture(GL_TEXTURE_2D, m_textureY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_frameWidth, m_frameHeight,
                 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yData);

    // 更新U纹理（缩放为1/2尺寸）
    glBindTexture(GL_TEXTURE_2D, m_textureU);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_frameWidth / 2, m_frameHeight,
                 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, uData);

    // 更新V纹理（缩放为1/2尺寸）
    glBindTexture(GL_TEXTURE_2D, m_textureV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_frameWidth / 2, m_frameHeight,
                 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, vData);

    // 释放临时数据
    delete[] yData;
    delete[] uData;
    delete[] vData;
}

void OpenGLVideoWidget::releaseGLResources()
{
    makeCurrent(); // 确保OpenGL上下文激活
    if (m_textureY) glDeleteTextures(1, &m_textureY);
    if (m_textureU) glDeleteTextures(1, &m_textureU);
    if (m_textureV) glDeleteTextures(1, &m_textureV);
    doneCurrent();
}

void OpenGLVideoWidget::updateYuyvFrame(const uchar *data, int width, int height)
{
    QMutexLocker locker(&m_mutex); // 加锁保证线程安全
    // 释放旧数据
    if (m_yuyvData) {
        delete[] m_yuyvData;
        m_yuyvData = nullptr;
    }
    // 拷贝新的YUYV数据
    m_frameWidth = width;
    m_frameHeight = height;
    int dataSize = width * height * 2; // YUYV是2字节/像素
    m_yuyvData = new uchar[dataSize];
    memcpy(m_yuyvData, data, dataSize);
    m_hasFrame = true;

    update();
}




