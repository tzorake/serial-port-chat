#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QTime>
#include <QDebug>
#include <QDataStream>
#include <cstring>
#include <QRegularExpression>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Serial Port Chat");

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos)
        ui->serialPortComboBox->addItem(info.portName());

    QStandardItemModel *receiveModel = new QStandardItemModel();
    populateModel(receiveModel, { DataType::String, DataType::Uint8Array, DataType::Uint16Array, DataType::Uint32Array, DataType::Float32Array, DataType::Float64Array });
    ui->receiveType->setModel(receiveModel);

    QStandardItemModel *sendModel = new QStandardItemModel();
    populateModel(sendModel, { DataType::String, DataType::Uint8Array, DataType::Uint16Array, DataType::Uint32Array, DataType::Float32Array, DataType::Float64Array });
    ui->sendType->setModel(sendModel);

    connect(ui->connectButton, &QPushButton::clicked, this, [this]() {
        m_serial.setPortName(ui->serialPortComboBox->currentText());

        if (!m_serial.open(QIODevice::ReadWrite)) {
            QMessageBox::critical(this, "Error", tr("Can't open %1, error code %2").arg(m_serial.portName()).arg(m_serial.error()));
            ui->connection->setEnabled(true);
            return;
        }

        ui->connection->setEnabled(false);

        ui->autoResponse->setEnabled(true);
        ui->receiveTypeConvertion->setEnabled(true);
        ui->sendTypeConvertion->setEnabled(true);
        ui->message->setEnabled(true);
        ui->sendButton->setEnabled(true);

        connect(&m_serial, &QSerialPort::readyRead, this, &MainWindow::readData);
        connect(ui->sendButton, &QPushButton::clicked, this, [this]() {
            auto text = ui->message->text().toUtf8();
            if (text.isEmpty()) {
                return;
            }

            writeData(text);
        });
    });

    connect(ui->autoResponseCheckbox, &QCheckBox::toggled, this, [this](bool toggled) {
        if (toggled) {
            m_autoResponseConnection = connect(&m_serial, &QSerialPort::readyRead, this, [this]() {
                auto text = ui->responseMessage->text();
                if (text.isEmpty()) {
                    return;
                }

                writeData(text);
            });
        }
        else {
            disconnect(m_autoResponseConnection);
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::populateModel(QStandardItemModel *model, QList<DataType> values)
{
    for (auto value : values) {
        auto item = new QStandardItem();
        item->setData(value, Qt::UserRole);
        item->setData(s_typeConversions[value], Qt::DisplayRole);
        model->appendRow(item);
    }
}

QString MainWindow::toReceiveType(QByteArray data)
{
    auto type = (DataType) ui->receiveType->currentData(Qt::UserRole).toInt();
    switch (type) {
        case DataType::String: {
            return QString::fromUtf8(data);
        } break;

        case DataType::Uint8Array: {
            QList<qint8> list;
            for (int i = 0; i < data.size(); i+=1) {
                list.append(static_cast<quint8>(data.at(i)));
            }

            QList<quint32> transformed;
            for (auto value : list) {
                transformed.append((quint32) value);
            }

            return listString(s_typeConversions[type], transformed);
        } break;

        case DataType::Uint16Array: {
            QList<qint16> list;
            for (int i = 0; i < data.size(); i+=2) {
                auto value = static_cast<qint16>(data[i])
                           | (static_cast<qint16>(data[i + 1] << 8));
                list.append(value);
            }

            QList<quint32> transformed;
            for (auto value : list) {
                transformed.append((quint32) value);
            }

            return listString(s_typeConversions[type], transformed);
        } break;

        case DataType::Uint32Array: {
            QList<quint32> list;
            for (int i = 0; i < data.size(); i+=4) {
                auto value = static_cast<quint32>(data[i])
                           | (static_cast<quint32>(data[i + 1] << 8))
                           | (static_cast<quint32>(data[i + 2] << 16))
                           | (static_cast<quint32>(data[i + 3] << 24));
                list.append(value);
            }

            return listString(s_typeConversions[type], list);
        } break;

        case DataType::Float32Array: {
            QList<float> list;

            int length = data.size() / sizeof(float);
            const char *constData = data.constData();

            for (int i = 0; i < length; i++) {
                float value;
                std::memcpy(&value, constData + i * sizeof(float), sizeof(float));
                list.append(value);
            }

            return listString(s_typeConversions[type], list);
        } break;

        case DataType::Float64Array: {
            QList<double> list;

            int length = data.size() / sizeof(double);
            const char *constData = data.constData();

            for (int i = 0; i < length; i++) {
                double value;
                std::memcpy(&value, constData + i * sizeof(double), sizeof(double));
                list.append(value);
            }

            return listString(s_typeConversions[type], list);
        } break;

        default: {
            qCritical() << "MainWindow::readData: There is no implementation for the passed value!";
        }
    }

    return "UnknownType()";
}

QByteArray MainWindow::toSendType(QString text)
{
    QByteArray byteArray;
    auto type = (DataType) ui->sendType->currentData(Qt::UserRole).toInt();

    switch (type) {
        case DataType::String: {
            byteArray.append(text.toUtf8());

            return byteArray;
        } break;

        case DataType::Uint8Array: {
            text = text.trimmed();

            if (text.isEmpty()) {
                return byteArray;
            }

            auto prefix = s_typeConversions[type];
            QRegularExpression regex(prefix + "\\(\\[(.*?)\\]\\)");
            QRegularExpressionMatch match = regex.match(text);
            if (match.hasMatch()) {
                QList<quint8> list;

                auto content = match.captured(1);
                auto numbers = content.split(",");
                for (const auto &number : numbers) {
                    bool ok = false;
                    auto value = number.toInt(&ok);
                    if (ok) {
                        list << value;
                    }
                }

                byteArray.reserve(list.size());
                for (const quint8 value : list) {
                    byteArray.append(value);
                }
            }

            return byteArray;
        } break;

        case DataType::Uint16Array: {
            text = text.trimmed();

            if (text.isEmpty()) {
                return byteArray;
            }

            auto prefix = s_typeConversions[type];

            QRegularExpression regex(prefix + "\\(\\[(.*?)\\]\\)");
            QRegularExpressionMatch match = regex.match(text);
            if (match.hasMatch()) {
                QList<quint16> list;

                auto content = match.captured(1);
                auto numbers = content.split(",");
                for (const auto &number : numbers) {
                    bool ok = false;
                    auto value = number.toInt(&ok);
                    if (ok) {
                        list << value;
                    }
                }

                byteArray.reserve(list.size() * sizeof(quint16));
                for (const quint16 value : list) {
                    byteArray.append(static_cast<char>(value & 0xFF));
                    byteArray.append(static_cast<char>(value >> 8) & 0xFF);
                }
            }

            return byteArray;
        } break;

        case DataType::Uint32Array: {
            text = text.trimmed();

            if (text.isEmpty()) {
                return byteArray;
            }

            auto prefix = s_typeConversions[type];

            QRegularExpression regex(prefix + "\\(\\[(.*?)\\]\\)");
            QRegularExpressionMatch match = regex.match(text);
            if (match.hasMatch()) {
                QList<quint32> list;

                auto content = match.captured(1);
                auto numbers = content.split(",");
                for (const auto &number : numbers) {
                    bool ok = false;
                    auto value = number.toInt(&ok);
                    if (ok) {
                        list << value;
                    }
                }

                byteArray.reserve(list.size() * sizeof(quint32));
                for (const quint32 value : list) {
                    byteArray.append(static_cast<char>(value & 0xFF));
                    byteArray.append(static_cast<char>(value >> 8) & 0xFF);
                    byteArray.append(static_cast<char>(value >> 16) & 0xFF);
                    byteArray.append(static_cast<char>(value >> 24) & 0xFF);
                }
            }

            return byteArray;
        } break;

        case DataType::Float32Array: {
            text = text.trimmed();

            if (text.isEmpty()) {
                return byteArray;
            }

            auto prefix = s_typeConversions[type];

            QRegularExpression regex(prefix + "\\(\\[(.*?)\\]\\)");
            QRegularExpressionMatch match = regex.match(text);
            if (match.hasMatch()) {
                QList<float> list;

                auto content = match.captured(1);
                auto numbers = content.split(",");
                for (const auto &number : numbers) {
                    bool ok = false;
                    auto value = number.toFloat(&ok);
                    if (ok) {
                        list << value;
                    }
                }

                byteArray.reserve(list.size() * sizeof(float));
                for (const float value : list) {
                    byteArray.append(reinterpret_cast<const char *>(&value), sizeof(float));
                }
            }

            return byteArray;
        } break;

        case DataType::Float64Array: {
            text = text.trimmed();

            if (text.isEmpty()) {
                return byteArray;
            }

            auto prefix = s_typeConversions[type];

            QRegularExpression regex(prefix + "\\(\\[(.*?)\\]\\)");
            QRegularExpressionMatch match = regex.match(text);
            if (match.hasMatch()) {
                QList<double> list;

                auto content = match.captured(1);
                auto numbers = content.split(",");
                for (const auto &number : numbers) {
                    bool ok = false;
                    auto value = number.toDouble(&ok);
                    if (ok) {
                        list << value;
                    }
                }

                byteArray.reserve(list.size() * sizeof(double));
                for (const double value : list) {
                    byteArray.append(reinterpret_cast<const char *>(&value), sizeof(double));
                }
            }

            return byteArray;
        } break;

        default: {
            qCritical() << "MainWindow::readData: There is no implementation for the passed value!";
        }
    }

    return byteArray;
}

void MainWindow::readData()
{
    ui->message->end(false);

    auto data = m_serial.readAll();
    auto convertedData = toReceiveType(data);

    ui->messages->insertHtml(tr("[%1] <font color=\"blue\">%2</font>").arg(QTime::currentTime().toString()).arg(convertedData));
    ui->messages->insertPlainText("\n");
}

void MainWindow::writeData(const QString &text)
{
    ui->message->end(false);
    auto data = text.toUtf8();
    auto convertedData = toSendType(data);

    m_serial.write(convertedData);
    ui->messages->insertHtml(tr("[%1] <font color=\"green\">%2</font>").arg(QTime::currentTime().toString()).arg(text));
    ui->messages->insertPlainText("\n");
}
