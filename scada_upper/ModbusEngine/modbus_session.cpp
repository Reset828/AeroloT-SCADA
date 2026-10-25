#include "modbus_session.h"

namespace ModbusEngine {

ModbusSession::ModbusSession(QObject* parent)
    : QObject(parent)
    , m_modbusEngine()
    , m_scanScheduler(&m_modbusEngine)
{
    connect(&m_modbusEngine, &ModbusEngine::connectionStateChanged,
            this, &ModbusSession::connectionStateChanged);
    connect(&m_modbusEngine, &ModbusEngine::modbusErrorOccurred,
            this, &ModbusSession::modbusErrorOccurred);
    connect(&m_modbusEngine, &ModbusEngine::dataReceived,
            this, &ModbusSession::dataReceived);
}

void ModbusSession::connectToHost(const QString& host, quint16 port)
{
    m_modbusEngine.connectToHost(host, port);
}

void ModbusSession::disconnectFromHost()
{
    m_scanScheduler.stop();
    m_modbusEngine.disconnectFromHost();
}

void ModbusSession::startScanning()
{
    m_scanScheduler.setOnlineDevices({1, 2, 3, 4, 5});
    m_scanScheduler.start();
}

void ModbusSession::stopScanning()
{
    m_scanScheduler.stop();
}

void ModbusSession::shutdown()
{
    m_scanScheduler.stop();
    m_modbusEngine.disconnectFromHost();
}

}
