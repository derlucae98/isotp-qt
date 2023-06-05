#ifndef CAN_H
#define CAN_H

#include <QObject>
#include <QCanBus>
#include <QDir>
#include <QProcess>
#include <QTimer>
#include <QDebug>


class Can : public QObject
{
    Q_OBJECT
public:
    explicit Can(QObject *parent = nullptr);
    ~Can();
    void init();

    void connect_device();
    void disconnect_device();
    void send_frame(QCanBusFrame frame);
    void set_device_name(QString deviceName);
    bool connect_socket();


private:

    static const QString serverName;
    QTimer *timeout = nullptr;
    QCanBusDevice *can_device = nullptr;

    void socket_received();
    void new_client_connected();
    void message_from_client();
    QString deviceName;
    void get_frame();
    void send_heartbeat();

signals:
    void new_frame(QCanBusFrame);
    void error(QString);
    void device_up();
    void device_down();
    void available_devices(QStringList);
};

#endif // CAN_H
