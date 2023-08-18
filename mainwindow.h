#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include <QMap>
#include <QStandardItemModel>
#include <QStringBuilder>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum DataType {
        String,
        Uint8Array,
        Uint16Array,
        Uint32Array,
        Float32Array,
        Float64Array,
    };
    Q_ENUM(DataType)
    
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void populateModel(QStandardItemModel *model, QList<DataType> values);

    template <typename T>
    QString listString(const QString &type, const QList<T> &list)
    {
        QList<QString> transformed;
        for (auto value : list) {
            transformed.append(QString::number(value));
        }

        QString stringified = type % "([" % transformed.join(" ,") % "])";

        return stringified;
    }

    QString toReceiveType(QByteArray data);
    QByteArray toSendType(QString data);

    static inline QMap<MainWindow::DataType, QString> s_typeConversions = {
        { String,       "String" },
        { Uint8Array,   "Uint8Array" },
        { Uint16Array,  "Uint16Array" },
        { Uint32Array,  "Uint32Array" },
        { Float32Array, "Float32Array" },
        { Float64Array, "Float64Array" },
    };

    QMetaObject::Connection m_autoResponseConnection;

    void readData();
    void writeData(const QString &text);

    QSerialPort m_serial;
    QTimer m_timer;

    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
