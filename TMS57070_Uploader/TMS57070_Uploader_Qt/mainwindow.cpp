#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSerialPortInfo>
#include <QSerialPort>
#include <QIODevice>
#include <QRegExp>
#include <QMessageBox>
#include <QThread>
#include <QTimer>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    init();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::init() {
    refreshSerialPortList();
    port.setBaudRate(250000);
    
    this->setWindowTitle("TMS57070 Uploader");

    updateTimer.start(16);
    connect(&updateTimer, &QTimer::timeout, this, &MainWindow::updateTimerSlot);
    connect(&port, &QSerialPort::readyRead, this, &MainWindow::serial_readyReadSlot);
}

void MainWindow::refreshSerialPortList() {
    //Empty the list
    ui->combo_port_selection->clear();

    //Repopulate the list
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ui->combo_port_selection->addItem(info.portName());
    }
}

bool MainWindow::connectSerial() {
    QString portName = ui->combo_port_selection->currentText();
    if (portName.length() == 0) {
        return false;
    }
    
    port.setPortName(portName);
    
    if (!port.open(QIODevice::ReadWrite)) {
        connected = false;
        return false;
    }
    connected = true;
    liveUpdateInProgress = true;
    QThread().msleep(100); //FIXME: hack to fix corrupt message when FW starts too late
    port.write("?????????"); //Synchronisation
    QThread().msleep(100); //FIXME: hack to fix corrupt message when FW starts too late
    port.write("?????????"); //Synchronisation
    updateViewBounds(ui->from_spinBox->value(), ui->to_spinBox->value());
    QThread().msleep(100); //FIXME: hack to fix corrupt message when FW starts too late
    updateViewBounds(ui->from_spinBox->value(), ui->to_spinBox->value());
    liveUpdateInProgress = false;
    
    return true;
}

void MainWindow::on_button_refresh_list_released() {
    refreshSerialPortList();
}

void MainWindow::on_button_serial_connect_released() {
    if (ui->button_serial_connect->text() == "Connect") {
        if (connectSerial()) {
            ui->button_serial_connect->setText("Disconnect");
        }
    } else {
        port.close();
        connected = false;
        liveUpdateInProgress = false;
        programmingInProgress = false;
        incomingMessage.clear();
        ui->button_serial_connect->setText("Connect");
    }
}

void MainWindow::serial_readyReadSlot() {
    //Read bytes until end-command char!
    //qDebug() << "ready read slot. at end: " << port.atEnd();
    while (!port.atEnd()) {
        QByteArray received = port.read(1);
        char newText = received.at(0);
        //qDebug() << "RX: <" << received[0] << "> (" << (int)received[0] << ")";
       
        if (newText == '!') {
            //message is complete
            interpretIncomingMessage(incomingMessage);
            incomingMessage = "";
        } else if (newText == '\n') {
            //Ignore anything that came before this
            incomingMessage = "";
        } else {
            incomingMessage += newText;
        }
    }
}

void MainWindow::interpretIncomingMessage(QString message) {
    qDebug() << "got message " << message;
    if (message == "p") {
        //PMEM upload complete
        if (programmingMemory == PROG_PMEM) {
            programmingInProgress = false;
        }
    } else if (message == "d") {
        //CMEM upload complete
        if (programmingMemory == PROG_CMEM) {
            programmingInProgress = false;
        }
    } else if (message[0] == 'r') {
        //process readout
        liveUpdateInProgress = false;
        message.remove(0, 1); //Remove command char
        qDebug() << "splitting message: " << message;
        QStringList hexes = message.split(" ");
        qDebug() << "split has " << hexes.length() << "items";
        
        int address = hexes[0].toInt();
        hexes.removeAt(0);
        
        updateCMEMView(address, hexes);
    }
}

void MainWindow::updateCMEMView(int address, QStringList hexes) {
    for (int i = 0; i < hexes.length() - 1; i++) { //-1 to ignore the trailing space
        int column = address & 0xF;
        int row = address >> 4;
        
        QTableWidgetItem* cell = ui->live_table->item(row, column);
        
        //Create cell if it doesn't exist already
        if (cell == nullptr) {
            cell = new QTableWidgetItem();
            ui->live_table->setItem(row, column, cell);
            cell->setBackground(QColor(255, 0, 0));
        } else {
            QString newtext = hexes[i];
            //If text has changed
            if (newtext != cell->text()) {
                //red background
                cell->setBackground(QColor(255, 0, 0));
            }
        }
        
        cell->setText(hexes[i]);
        
        address++;
    }
    
    //Update activity colours
    for (int i = 0; i < 256; i++) {
        int column = i & 0xF;
        int row = i >> 4;
        
        QTableWidgetItem* cell = ui->live_table->item(row, column);
        if (cell != nullptr) {
            QColor colour = cell->background().color();
            int nonred = colour.blue() + 25;
            if (nonred > 255)
                nonred = 255;
            
            cell->setBackground(QColor(255, nonred, nonred));
        } else {
            //Abort loop if there is no cell
            break;
        }
    }
}

QPair<QStringList, QString> MainWindow::formatHexInput(QString programText, unsigned int numberlength) {
    //Remove comments
    bool justSeenSlash = false;
    for (int i = 0; i < programText.length(); i++) {
//        qDebug() << "Seen " << programText[i];
        if (programText[i] == '/') {
            if (justSeenSlash) {
                justSeenSlash = false;
                //Comment is starting here, delet it!
                //Scan for next newline
                int EOL = programText.length() - 1; //Default value for when we don't find an EOL
                for (int ii = i; ii < programText.length(); ii++) {
                    if (programText[ii] == '\n') {
                        //EOL found
                        EOL = ii;
                        break;
                    } else if (programText[ii] == '\r') {
                        //Is the next character a \n?
                        if ((programText.length() > ii + 1) && (programText[ii+1] == '\n')) {
                            EOL = ii + 1;
                        } else {
                            EOL = ii;
                        }
                        break;
                    }
                }
                i--; //Put us back on the first slash character
                programText.remove(i, EOL - i + 1); //Cut out comment
                i--; //Put the next loop iteration on the first slash character

            } else {
                justSeenSlash = true;
            }
        } else {
            justSeenSlash = false;
        }
    }

    //Remove whitespace
    for (int i = 0; i < programText.length(); i++) {
        if (programText[i].isSpace()) {
            programText.remove(i, 1);
            i--;
        }
    }

    //printf("text: %s\n", programText.toUtf8().constData()); //debug

    QStringList instructions = programText.split(",");

    //Fill in NOPs with 00003000
    for (int i = 0; i < instructions.length(); i++) {
        if (instructions[i] == "NOP") {
            instructions[i] = "00003000";
        }
    }

    //Strip 0x from instructions
    for (int i = 0; i < instructions.length(); i++) {
        if (instructions[i].indexOf("0x") == 0) {
            instructions[i].remove(0, 2);
        }
    }

    for (int i = 0; i < instructions.length(); i++) {
//        qDebug() << instructions[i] << ", ";
    }

    //Check all instructions are valid
    for (int i = 0; i < instructions.length(); i++) {
        //Does it only contain hex chars?
        if (instructions[i].count(QRegExp("[a-fA-F0-9]")) != instructions[i].length()) {
            QString error = QString("Non-hex character in instruction: %1").arg(instructions[i].toUtf8().constData());
            return QPair<QStringList, QString>(instructions, error);
        }

        //Is it too long?
        if (instructions[i].length() != numberlength) {
            QString error = QString("Instruction is wrong length: %1").arg(instructions[i].toUtf8().constData());
            return QPair<QStringList, QString>(instructions, error);
        }
    }
    return QPair<QStringList, QString>(instructions, "");
}

bool MainWindow::sendStringList(QChar command, QStringList data) {
    if (!connected)
        return false;
    
    //TODO: check if port.write fails
    QString message;
    message += command;
    for (int i = 0; i < data.length(); i++) {
        message += data[i];
        message += ',';
    }
    message += "!,"; //finished
    
    port.write(message.toUtf8());
    qDebug() << "TX: " << message;

    return true;
}

//Parse program and data and send them over serial
void MainWindow::on_button_upload_released() {
    //Parse program
    QString programText = ui->program_textBox->toPlainText();

    QPair<QStringList, QString> program = formatHexInput(programText, 8);
    QStringList instructions = program.first;
    QString error = program.second;

    if (error != "") {
        //Show error in some fashion
        QMessageBox info;
        info.setText(tr("Error loading program from text"));
        info.setInformativeText(error);
        info.setStandardButtons(QMessageBox::Ok);
        info.exec();
        return;
    }

    //Parse data
    QString dataText = ui->data_textBox->toPlainText();

    QPair<QStringList, QString> data = formatHexInput(dataText, 6);
    QStringList datas = data.first;
    error = data.second;

    if (error != "") {
        //Show error in some fashion
        QMessageBox info;
        info.setText(tr("Error loading CMEM from text"));
        info.setInformativeText(error);
        info.setStandardButtons(QMessageBox::Ok);
        info.exec();
        return;
    }
    //Don't request CMEM updates while uploading
    programmingInProgress = true;
    programmingMemory = PROG_CMEM; //CMEM is second, so programming is over when CMEM is over

    //Send them on over
    if (!sendStringList('p', instructions)) {
        QMessageBox info;
        info.setText(tr("Sending PMEM failed"));
        info.setStandardButtons(QMessageBox::Ok);
        info.exec();
        programmingInProgress = false;
        return;
    }
    
    QThread().msleep(200); //FIXME: hack to let PMEM programming happen before sending CMEM

    if (!sendStringList('d', datas)) {
        QMessageBox info;
        info.setText(tr("Sending CMEM failed"));
        info.setStandardButtons(QMessageBox::Ok);
        info.exec();
        programmingInProgress = false;
        return;
    }
}

void MainWindow::on_program_pushButton_released() {
    //Parse program
    QString programText = ui->program_textBox->toPlainText();

    QPair<QStringList, QString> program = formatHexInput(programText, 8);
    QStringList instructions = program.first;
    QString error = program.second;

    if (error != "") {
        //Show error in some fashion
        QMessageBox info;
        info.setText(tr("Error loading program from text"));
        info.setInformativeText(error);
        info.setStandardButtons(QMessageBox::Ok);
        info.exec();
        return;
    }
    //Don't request CMEM updates while uploading
    programmingInProgress = true;
    programmingMemory = PROG_PMEM;

    //Send them on over
    if (!sendStringList('p', instructions)) {
        QMessageBox info;
        info.setText(tr("Sending PMEM failed"));
        info.setStandardButtons(QMessageBox::Ok);
        info.exec();
    }
}

void MainWindow::on_data_pushButton_released() {
    //Parse data
    QString dataText = ui->data_textBox->toPlainText();

    QPair<QStringList, QString> data = formatHexInput(dataText, 6);
    QStringList datas = data.first;
    QString error = data.second;

    if (error != "") {
        //Show error in some fashion
        QMessageBox info;
        info.setText(tr("Error loading CMEM from text"));
        info.setInformativeText(error);
        info.setStandardButtons(QMessageBox::Ok);
        info.exec();
        return;
    }
    //Don't request CMEM updates while uploading
    programmingInProgress = true;
    programmingMemory = PROG_CMEM;

    //Send them on over
    if (!sendStringList('d', datas)) {
        QMessageBox info;
        info.setText(tr("Sending CMEM failed"));
        info.setStandardButtons(QMessageBox::Ok);
        info.exec();
    }
}

void MainWindow::updateTimerSlot() {
    if (!connected)
        return;
    
//    qDebug() << "updateTimerSlot: programmingInProgress = " << programmingInProgress;
    
    if (programmingInProgress)
        return;
    
    if (!liveUpdate)
        return;
    
//    qDebug() << "updateTimerSlot: liveUpdateInProgress = " << liveUpdateInProgress;
    
    if (!liveUpdateInProgress) {
        liveUpdateInProgress = true;
        port.write("r");
        updateSpinner();
    }
}

void MainWindow::updateSpinner() {
    QString current = ui->label_updateSpinner->text();
    switch (current.at(0).toLatin1()) {
    case '|':  current = "/"; break;
    case '/':  current = "-"; break;
    case '-':  current = "\\"; break;
    case '\\': current = "|"; break;
    default:   current = "/"; break;
    }
    ui->label_updateSpinner->setText(current);
}

void MainWindow::on_checkBox_stateChanged(int checked) {
    liveUpdate = checked == Qt::Checked;
}

void MainWindow::updateViewBounds(int from, int to) {
    QStringList list;
    list += QString("%1").arg(from, 2, 16);
    list += QString("%1").arg(to, 2, 16);
    
    sendStringList('s', list);
}
void MainWindow::on_from_spinBox_valueChanged(int value) {
    updateViewBounds(value, ui->to_spinBox->value());
}
void MainWindow::on_to_spinBox_valueChanged(int value) {
    updateViewBounds(ui->from_spinBox->value(), value);
}

void MainWindow::on_pushButton_resize_clicked() {
    QHeaderView* header = ui->live_table->horizontalHeader();
    if (header == nullptr) return;
    
    for (int i = 1; i < header->count(); i++) {
        header->resizeSection(i, header->sectionSize(0));
    }
}




































