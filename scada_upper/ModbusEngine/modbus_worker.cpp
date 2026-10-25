#include "modbus_worker.h"
#include <QModbusDataUnit>
#include <QDebug>

namespace ModbusEngine {

ModbusWorker::ModbusWorker(QObject* parent)
    : QObject(parent)
    , m_modbusClient(nullptr)
    , m_commandQueue(nullptr)
    , m_reconnectTimer(nullptr)
    , m_requestTimeoutTimer(nullptr)
    , m_connectionState(core::ConnectionState::Disconnected)
    , m_requestInFlight(false)
    , m_currentReply(nullptr)
    , m_consecutiveTimeouts(0)
{
    m_modbusClient = new QModbusTcpClient(this);
    m_commandQueue = new CommandQueue(this);
    m_reconnectTimer = new QTimer(this);
    m_requestTimeoutTimer = new QTimer(this);

    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(5000);

    m_requestTimeoutTimer->setSingleShot(true);
    m_requestTimeoutTimer->setInterval(1500);

    connect(m_modbusClient, &QModbusTcpClient::stateChanged,
            this, &ModbusWorker::onStateChanged);
    connect(m_modbusClient, &QModbusTcpClient::errorOccurred,
            this, &ModbusWorker::onErrorOccurred);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &ModbusWorker::onReconnectTimerTimeout);
    connect(m_requestTimeoutTimer, &QTimer::timeout,
            this, &ModbusWorker::onRequestTimeout);

    emit connectionReady();
}

ModbusWorker::~ModbusWorker() {
    if (m_modbusClient) {
        m_modbusClient->disconnectDevice();
    }
}

void ModbusWorker::setState(core::ConnectionState state) {
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

void ModbusWorker::startConnection(const QString& host, quint16 port) {
    m_host = host;
    m_port = port;

    setState(core::ConnectionState::Connecting);

    m_modbusClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, host);
    m_modbusClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    m_modbusClient->setTimeout(1000);
    m_modbusClient->setNumberOfRetries(0);
    m_modbusClient->connectDevice();
}

void ModbusWorker::stopConnection() {
    m_reconnectTimer->stop();
    m_requestTimeoutTimer->stop();
    m_commandQueue->clear();
    m_requestInFlight = false;
    m_currentReply = nullptr;
    m_modbusClient->disconnectDevice();
    setState(core::ConnectionState::Disconnected);
}

void ModbusWorker::pushReadCommand(const Command& cmd) {
    m_commandQueue->pushRead(cmd);
    if (!m_requestInFlight && m_connectionState == core::ConnectionState::Connected) {
        sendNextCommand();
    }
}

void ModbusWorker::pushWriteCommand(const Command& cmd) {
    m_commandQueue->pushWrite(cmd);
    if (!m_requestInFlight && m_connectionState == core::ConnectionState::Connected) {
        sendNextCommand();
    }
}

void ModbusWorker::checkConnection() {
    if (m_connectionState == core::ConnectionState::Reconnecting) {
        setState(core::ConnectionState::Connecting);
        m_modbusClient->connectDevice();
    }
}

void ModbusWorker::onStateChanged(QModbusDevice::State state) {
    switch (state) {
    case QModbusDevice::ConnectingState:
        setState(core::ConnectionState::Connecting);
        break;
    case QModbusDevice::ConnectedState:
        setState(core::ConnectionState::Connected);
        m_reconnectTimer->stop();
        m_consecutiveTimeouts = 0;
        if (!m_requestInFlight) {
            sendNextCommand();
        }
        break;
    case QModbusDevice::UnconnectedState:
    case QModbusDevice::ClosingState:
        if (m_connectionState == core::ConnectionState::Connecting ||
            m_connectionState == core::ConnectionState::Connected) {
            setState(core::ConnectionState::Reconnecting);
            m_requestInFlight = false;
            m_reconnectTimer->start();
        }
        break;
    default:
        break;
    }
}

void ModbusWorker::onErrorOccurred(QModbusDevice::Error error) {
    Q_UNUSED(error);
    QString errorString = m_modbusClient->errorString();
    if (!errorString.isEmpty()) {
        emit modbusErrorOccurred(errorString);
    }
}

void ModbusWorker::onReplyFinished() {
    auto* reply = qobject_cast<QModbusReply*>(sender());
    if (!reply) return;

    handleReply(reply);

    m_requestInFlight = false;
    sendNextCommand();
}

void ModbusWorker::onRequestTimeout() {
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    m_requestInFlight = false;
    m_consecutiveTimeouts++;
    checkConnectionLost();
    sendNextCommand();
}

void ModbusWorker::onReconnectTimerTimeout() {
    if (m_connectionState == core::ConnectionState::Reconnecting) {
        setState(core::ConnectionState::Connecting);
        m_modbusClient->connectDevice();
    }
}

void ModbusWorker::sendNextCommand() {
    if (m_requestInFlight) {
        return;
    }

    if (m_commandQueue->isEmpty()) {
        return;
    }

    if (m_connectionState != core::ConnectionState::Connected) {
        return;
    }

    Command cmd = m_commandQueue->pop();

    QModbusDataUnit::RegisterType regType;
    switch (cmd.registerType) {
    case core::ModbusRegisterType::Coil:
        regType = QModbusDataUnit::Coils;
        break;
    case core::ModbusRegisterType::Discrete:
        regType = QModbusDataUnit::DiscreteInputs;
        break;
    case core::ModbusRegisterType::Holding:
        regType = QModbusDataUnit::HoldingRegisters;
        break;
    case core::ModbusRegisterType::Input:
        regType = QModbusDataUnit::InputRegisters;
        break;
    default:
        sendNextCommand();
        return;
    }

    QModbusReply* reply = nullptr;

    if (cmd.type == CommandType::Read) {
        QModbusDataUnit unit(regType, cmd.startAddress, cmd.quantity);
        reply = m_modbusClient->sendReadRequest(unit, cmd.slaveId);
    } else if (cmd.type == CommandType::Write) {
        QModbusDataUnit unit(regType, cmd.startAddress, cmd.quantity);
        for (int i = 0; i < cmd.writeValues.size() && i < cmd.quantity; ++i) {
            unit.setValue(i, cmd.writeValues[i].toUInt());
        }
        reply = m_modbusClient->sendWriteRequest(unit, cmd.slaveId);
    }

    if (reply) {
        m_requestInFlight = true;
        m_currentReply = reply;
        m_requestTimeoutTimer->start();
        connect(reply, &QModbusReply::finished, this, &ModbusWorker::onReplyFinished);
    } else {
        sendNextCommand();
    }
}

void ModbusWorker::handleReply(QModbusReply* reply) {
    int slaveId = reply->serverAddress();

    m_requestTimeoutTimer->stop();

    if (reply->error() != QModbusDevice::NoError) {
        m_consecutiveTimeouts++;
        checkConnectionLost();
    } else {
        m_consecutiveTimeouts = 0;
    }

    if (reply->result().isValid()) {
        const QModbusDataUnit unit = reply->result();

        QVector<quint16> values;
        for (uint i = 0; i < unit.valueCount(); ++i) {
            values.append(unit.value(i));
        }

        emit dataReceived(slaveId, values, unit.registerType());
    }

    reply->deleteLater();
    m_currentReply = nullptr;
}

void ModbusWorker::checkConnectionLost() {
    if (m_consecutiveTimeouts >= MAX_TIMEOUTS &&
        m_connectionState == core::ConnectionState::Connected) {

        setState(core::ConnectionState::Reconnecting);
        m_requestInFlight = false;
        m_consecutiveTimeouts = 0;

        m_modbusClient->disconnectDevice();
        m_reconnectTimer->start();
    }
}

}
