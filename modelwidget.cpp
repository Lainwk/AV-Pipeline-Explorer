#include "modelwidget.h"

ModelWidget::ModelWidget(QWidget *parent)
{

}

void ModelWidget::setCurrentOutputPath(const QString &newCurrentOutputPath)
{
    this->currentOutputPath = newCurrentOutputPath;
}

void ModelWidget::set_select_device(const selectedDeviceV4L2Params &newCurrentVideoDeviceParams, const QString &newSelected_audio_device)
{
    currentVideoDeviceParams = newCurrentVideoDeviceParams;
    selected_audio_device = newSelected_audio_device;
}

void ModelWidget::showMessageBox(QWidget *parent, const QString &title, const QString &message)
{
    QMessageBox::information(parent,title,message);
}

void ModelWidget::showWarningBox(QWidget *parent, const QString &title, const QString &message)
{
    QMessageBox::warning(parent,title,message);
}

