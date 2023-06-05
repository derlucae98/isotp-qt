#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "isotp.h"
#include <QTimer>
#include "can.h"

#define RECV_BUF_SIZE 4095
#define SEND_BUF_SIZE 4095
#define ISOTP_UPLINK 0x013
#define ISOTP_DOWNLINK 0x012

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    Isotp *isotp = nullptr;
    void can_send(QCanBusFrame frame);
    void connect_can();
    QCanBusDevice *can_device = nullptr;
    void can_recv();
    void on_new_isotp_message(QByteArray message);

signals:
    void new_message(QCanBusFrame);
private slots:
    void on_pushButton_clicked();
};
#endif // MAINWINDOW_H
