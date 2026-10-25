#include "modbus_engine.h"
namespace ModbusEngine {

ModbusEngine::ModbusEngine(QObject* parent)
    : QObject(parent)
    , m_thread(new QThread(this))
    , m_worker(new ModbusWorker())
    , m_connectionState(core::ConnectionState::Disconnected)
{
    m_worker->moveToThread(m_thread);

    connect(m_worker, &ModbusWorker::connectionStateChanged,
            this, &ModbusEngine::onWorkerConnectionStateChanged,
            Qt::QueuedConnection);
    connect(m_worker, &ModbusWorker::modbusErrorOccurred,
            this, &ModbusEngine::modbusErrorOccurred,
            Qt::QueuedConnection);
    connect(m_worker, &ModbusWorker::dataReceived,
            this, &ModbusEngine::dataReceived,
            Qt::QueuedConnection);

    connect(m_thread, &QThread::finished,
            m_worker, &QObject::deleteLater);

    m_thread->start();
}

ModbusEngine::~ModbusEngine() {
    if (m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }
}

void ModbusEngine::connectToHost(const QString& host, quint16 port) {
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, host, port]() {
        worker->startConnection(host, port);
    }, Qt::QueuedConnection);
}

void ModbusEngine::disconnectFromHost() {
    QMetaObject::invokeMethod(m_worker, [worker = m_worker]() {
        worker->stopConnection();
    }, Qt::QueuedConnection);
}

void ModbusEngine::pushRead(const Command& cmd) {
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, cmd]() {
        worker->pushReadCommand(cmd);
    }, Qt::QueuedConnection);
}

void ModbusEngine::pushWrite(const Command& cmd) {
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, cmd]() {
        worker->pushWriteCommand(cmd);
    }, Qt::QueuedConnection);
}

bool ModbusEngine::isConnected() const {
    return m_connectionState == core::ConnectionState::Connected;
}

void ModbusEngine::onWorkerConnectionStateChanged(core::ConnectionState state) {
    m_connectionState = state;
    emit connectionStateChanged(state);
}

}
