#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "isotp.h"
#include <QTimer>
#include "can.h"

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
    QTimer *timer = nullptr;
    void connect_can();
    QCanBusDevice *can_device = nullptr;
    void can_recv();

signals:
    void new_message(QCanBusFrame);
private slots:
    void on_pushButton_clicked();
};
#endif // MAINWINDOW_H
