#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect_can();

    isotp = new Isotp(this);

    QObject::connect(isotp, &Isotp::send_can, this, &MainWindow::can_send);
    QObject::connect(this, &MainWindow::new_message, isotp, &Isotp::on_can_message);

    isotp->init_link(ISOTP_UPLINK, SEND_BUF_SIZE, RECV_BUF_SIZE, 10);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::can_send(QCanBusFrame frame)
{
    if (can_device && can_device->state() == QCanBusDevice::ConnectedState) {
        can_device->writeFrame(frame);
    }
}

void MainWindow::connect_can()
{
    QString errorString;
    can_device = QCanBus::instance()->createDevice(
        QStringLiteral("socketcan"), QString("can0"), &errorString);
    can_device->setConfigurationParameter(QCanBusDevice::BitRateKey, 1000000);
    if (!can_device) {
        qDebug("Can device init failed");
    }
    if (!can_device->connectDevice()) {
        qDebug("Can device init failed");
    }
    QObject::connect(can_device, &QCanBusDevice::framesReceived, this, &MainWindow::can_recv);
    qDebug("Can init successful");
}

void MainWindow::can_recv()
{
    while (can_device->framesAvailable()) {
        QCanBusFrame frame = can_device->readFrame();
        if (frame.frameId() == ISOTP_DOWNLINK) {
            emit new_message(frame);
        }
    }
}

void MainWindow::on_pushButton_clicked()
{
    QByteArray payload;
    payload.append(0x55);
    payload.append(0x5A);
    payload.append(0xA5);
    payload.append(0xAA);
    payload.append(0xBB);
    payload.append(0x12);
    payload.append(0x01);
    payload.append(0xFF);
    payload.append(0x74);
    payload.append(0x9C);
    payload.append(0x1B);
    payload.append(0x06);
    isotp->send(payload);
}

