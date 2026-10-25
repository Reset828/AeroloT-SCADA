#ifndef MODBUSENGINE_MODBUS_SESSION_H
#define MODBUSENGINE_MODBUS_SESSION_H

#include <QObject>

#include "modbus_engine.h"
#include "scan_scheduler.h"

namespace ModbusEngine {

class ModbusSession : public QObject {
    Q_OBJECT

public:
    explicit ModbusSession(QObject* parent = nullptr);

    void connectToHost(const QString& host, quint16 port);
    void disconnectFromHost();
    void startScanning();
    void stopScanning();
    void shutdown();

signals:
    void connectionStateChanged(core::ConnectionState state);
    void modbusErrorOccurred(const QString& errorString);
    void dataReceived(int slaveId, const QVector<quint16>& values,
                      QModbusDataUnit::RegisterType registerType);

private:
    ModbusEngine m_modbusEngine;
    ScanScheduler m_scanScheduler;
};

}

#endif
