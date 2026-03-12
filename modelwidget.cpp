#include "modelwidget.h"

ModelWidget::ModelWidget(QWidget *parent)
    : QWidget{parent}
{


}

void ModelWidget::set_selected_device(const QString &videoDevice, const QString &audioDevice)
{
    this->selected_video_device = videoDevice;
    this->selected_audio_device = audioDevice;
}
