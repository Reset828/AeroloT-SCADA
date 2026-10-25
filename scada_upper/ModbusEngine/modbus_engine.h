#ifndef MODBUSENGINE_MODBUSENGINE_H
#define MODBUSENGINE_MODBUSENGINE_H

#include <QObject>
#include <QThread>
#include "command_queue.h"
#include "modbus_worker.h"
#include "core/types.h"

namespace ModbusEngine {

class ModbusEngine : public QObject {
    Q_OBJECT
public:
    explicit ModbusEngine(QObject* parent = nullptr);
    ~ModbusEngine();

    void connectToHost(const QString& host, quint16 port);
    void disconnectFromHost();

    void pushRead(const Command& cmd);
    void pushWrite(const Command& cmd);

    bool isConnected() const;

signals:
    void connectionStateChanged(core::ConnectionState state);
    void modbusErrorOccurred(const QString& errorString);
    void dataReceived(int slaveId, const QVector<quint16>& values, QModbusDataUnit::RegisterType registerType);

private slots:
    void onWorkerConnectionStateChanged(core::ConnectionState state);

private:
    QThread* m_thread;
    ModbusWorker* m_worker;
    core::ConnectionState m_connectionState;
};

}

#endif
