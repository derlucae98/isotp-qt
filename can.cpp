#include "can.h"

Can::Can(QObject *parent) : QObject(parent)
{

}

Can::~Can()
{

}

void Can::send_frame(QCanBusFrame frame)
{
    if (can_device) {
        can_device->writeFrame(frame);
    }
}

void Can::set_device_name(QString deviceName)
{
    this->deviceName = deviceName;
}

bool Can::connect_socket()
{

}

void Can::get_frame()
{
    while(can_device->framesAvailable()) {
        emit new_frame(can_device->readFrame());
    }
}
