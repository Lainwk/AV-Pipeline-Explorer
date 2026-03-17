# 基础模块（保留）
QT       += core gui

# 区分Qt5/Qt6的OpenGL模块（关键！避免版本兼容问题）
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
    # Qt5 vs Qt6 差异化配置
    equals(QT_MAJOR_VERSION, 5) {
        # Qt5：openglwidgets 需 Qt 5.10+，且依赖 opengl 模块
        QT += opengl openglwidgets
        # Qt5 需显式链接 OpenGL 库（WSL2下必需）
        LIBS += -lGL -lGLU -lGLEW
    }
    equals(QT_MAJOR_VERSION, 6) {
        # Qt6：openglwidgets 已整合到 Qt6OpenGLWidgets，无需额外链接系统库
        QT += openglwidgets
        # Qt6 自动链接OpenGL，无需手动加 LIBS += -lGL
        LIBS -= -lGL -lGLU
    }
}

# C++版本升级到17（适配现代Qt+WSL2，兼容11）
CONFIG += c++17

# 可选：显式指定OpenGL版本（避免WSL2下版本兼容问题）
DEFINES += QT_OPENGL_ES_2 QT_OPENGL_VERSION=460

# 原有配置保留（源文件/头文件/表单）
SOURCES += \
    devicemanagewidget.cpp \
    main.cpp \
    mainwindow.cpp \
    modelwidget.cpp \
    v4l2capturewidget.cpp \
    v4l2previewthread.cpp

HEADERS += \
    devicemanagewidget.h \
    mainwindow.h \
    modelwidget.h \
    v4l2capturewidget.h \
    v4l2previewthread.h

FORMS += \
    devicemanagewidget.ui \
    mainwindow.ui \
    v4l2capturewidget.ui

# 部署规则（保留）
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
