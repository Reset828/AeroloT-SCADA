#include "scan_scheduler.h"
#include <QDebug>

namespace ModbusEngine {

ScanScheduler::ScanScheduler(ModbusEngine* engine, QObject* parent)
    : QObject(parent)
    , m_modbusEngine(engine)
    , m_scanTimer(new QTimer(this))
    , m_scanIntervalMs(500)
{
    m_onlineDevices = {1, 2, 3, 4, 5};

    m_scanTimer->setInterval(m_scanIntervalMs);
    connect(m_scanTimer, &QTimer::timeout, this, &ScanScheduler::onScanTimeout);
}

ScanScheduler::~ScanScheduler() {
    stop();
}

void ScanScheduler::start() {
    if (!m_scanTimer->isActive()) {
        m_scanTimer->start();
    }
}

void ScanScheduler::stop() {
    if (m_scanTimer->isActive()) {
        m_scanTimer->stop();
    }
}

void ScanScheduler::setScanInterval(int intervalMs) {
    m_scanIntervalMs = intervalMs;
    m_scanTimer->setInterval(intervalMs);
}

void ScanScheduler::setOnlineDevices(const QList<int>& deviceIds) {
    m_onlineDevices = deviceIds;
}

void ScanScheduler::onScanTimeout() {
    scanAllDevices();
    emit scanTriggered(m_onlineDevices.count());
}

void ScanScheduler::scanAllDevices() {
    for (int deviceId : m_onlineDevices) {
        std::vector<ReadRequest> groups = RegisterMap::buildReadGroups(deviceId);

        for (const auto& group : groups) {
            Command cmd;
            cmd.type = CommandType::Read;
            cmd.registerType = group.registerType;
            cmd.slaveId = group.slaveId;
            cmd.startAddress = group.startAddress;
            cmd.quantity = group.quantity;

            m_modbusEngine->pushRead(cmd);
        }
    }
}

}
