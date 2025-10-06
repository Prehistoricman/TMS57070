#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

typedef enum {
    PROG_PMEM,
    PROG_CMEM,
} programming_memory_t;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_button_refresh_list_released();
    void on_button_serial_connect_released();
    
    void serial_readyReadSlot();
    
    void on_button_upload_released();
    
    void on_program_pushButton_released();
    
    void on_data_pushButton_released();
    
    void updateTimerSlot();
    
    void on_checkBox_stateChanged(int);
    
    void on_from_spinBox_valueChanged(int);
    void on_to_spinBox_valueChanged(int);
    
    void on_pushButton_resize_clicked();
    
private:
    Ui::MainWindow *ui;
    void refreshSerialPortList();
    void init();
    bool connectSerial();
    QPair<QStringList, QString> formatHexInput(QString, unsigned int);
    bool sendStringList(QChar command, QStringList data);
    void interpretIncomingMessage(QString message);
    void updateViewBounds(int from, int to);
    void updateCMEMView(int address, QStringList hexes);
    void updateSpinner();
    
    QSerialPort port;
    bool connected = false;
    QString incomingMessage;

    //QThread* thread_comms;
    //Comms* comms;

    bool liveUpdate = false;
    bool liveUpdateInProgress = false;
    bool programmingInProgress = false;
    programming_memory_t programmingMemory;
    QTimer updateTimer;
};
#endif // MAINWINDOW_H
