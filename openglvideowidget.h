#ifndef OPENGLVIDEOWIDGET_H
#define OPENGLVIDEOWIDGET_H

#include <QObject>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QMutex>
#include <QPainter>


class OpenGLVideoWidget: public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit OpenGLVideoWidget(QWidget *parent = nullptr);
    ~OpenGLVideoWidget() override;

protected:
    void initializeGL() override;   // 初始化OpenGL上下文
    void resizeGL(int w, int h) override; // 窗口大小变化时调整视口
    void paintGL() override;        // 绘制帧数据

private:
    QMutex m_mutex;                 // 线程安全锁（防止帧数据读写冲突）
    uchar *m_yuyvData = nullptr;    // YUYV原始数据缓存
    int m_frameWidth = 640;         // 帧宽度
    int m_frameHeight = 480;        // 帧高度
    GLuint m_textureY = 0;          // Y分量纹理
    GLuint m_textureU = 0;          // U分量纹理
    GLuint m_textureV = 0;          // V分量纹理
    bool m_hasFrame = false;        // 是否有有效帧数据

    // YUYV转YUV分量并绑定纹理
    void updateYuvTextures();
    // 释放OpenGL资源
    void releaseGLResources();

private slots:
    void updateYuyvFrame(const uchar *data, int width, int height);

signals:
    void add_Logs(const QString &logmessage);

};

#endif // OPENGLVIDEOWIDGET_H
