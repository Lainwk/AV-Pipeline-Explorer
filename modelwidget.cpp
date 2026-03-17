#include "modelwidget.h"

ModelWidget::ModelWidget(QWidget *parent)
{

}

void ModelWidget::set_select_device(const selectedDeviceV4L2Params &newCurrentVideoDeviceParams, const QString &newSelected_audio_device)
{
    currentVideoDeviceParams = newCurrentVideoDeviceParams;
    selected_audio_device = newSelected_audio_device;
}
