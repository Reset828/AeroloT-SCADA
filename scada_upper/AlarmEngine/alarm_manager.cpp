#include "alarm_manager.h"
#include <QDebug>
#include <QDateTime>

namespace AlarmEngine {

AlarmManager::AlarmManager() {
}

AlarmManager& AlarmManager::instance() {
    static AlarmManager inst;
    return inst;
}

void AlarmManager::initialize() {
    AlarmConfigManager::instance().initialize();
    AlarmConfigManager::instance().applyToEvaluator(m_evaluator);
}

void AlarmManager::setAlarmRecordWriter(Application::AlarmRecordWriter& writer)
{
    m_alarmRecordWriter = &writer;
}

void AlarmManager::processDeviceData(const core::DeviceData& data)
{
    processDeviceData(data.deviceId, static_cast<int>(data.deviceType),
                      data.pressure, data.temperature, data.phaseCurrent,
                      data.outletPressure, data.dewPoint);
}

void AlarmManager::processDeviceData(int deviceId, int deviceType,
                                      double pressure, double temperature,
                                      double current, double outletPressure,
                                      double dewPoint) {
    if (deviceId >= 1 && deviceId <= 3) {
        evaluateCompressor(deviceId, temperature, pressure, current);
    } else if (deviceId == 4) {
        evaluateTank(deviceId, pressure, temperature);
    } else if (deviceId == 5) {
        evaluateDryer(deviceId, pressure, outletPressure, dewPoint);
    }
}

void AlarmManager::evaluateCompressor(int deviceId, double temperature, double pressure, double current) {
    AlarmEvent event;

    if (m_evaluator.evaluate(deviceId, 1, temperature, event)) {
        recordAlarmTransition(deviceId, 0, 2, 2, event,
                              QString("温度超限: %1°C").arg(temperature, 0, 'f', 1),
                              QString("温度恢复正常: %1°C").arg(temperature, 0, 'f', 1));
    }

    if (m_evaluator.evaluate(deviceId, 2, pressure, event)) {
        recordAlarmTransition(deviceId, 0, 1, 2, event,
                              QString("排气压力超限: %1bar").arg(pressure, 0, 'f', 2),
                              QString("排气压力恢复正常: %1bar").arg(pressure, 0, 'f', 2));
    }

    if (m_evaluator.evaluate(deviceId, 3, current, event)) {
        recordAlarmTransition(deviceId, 0, 3, 1, event,
                              QString("运行电流超限: %1A").arg(current, 0, 'f', 1),
                              QString("运行电流恢复正常: %1A").arg(current, 0, 'f', 1));
    }
}

void AlarmManager::evaluateTank(int deviceId, double pressure, double temperature) {
    AlarmEvent event;

    if (m_evaluator.evaluate(deviceId, 1, pressure, event)) {
        recordAlarmTransition(deviceId, 1, 1, 2, event,
                              QString("罐压超限: %1bar").arg(pressure, 0, 'f', 2),
                              QString("罐压恢复正常: %1bar").arg(pressure, 0, 'f', 2));
    }

    if (m_evaluator.evaluate(deviceId, 2, temperature, event)) {
        recordAlarmTransition(deviceId, 1, 2, 1, event,
                              QString("温度超限: %1°C").arg(temperature, 0, 'f', 1),
                              QString("温度恢复正常: %1°C").arg(temperature, 0, 'f', 1));
    }
}

void AlarmManager::evaluateDryer(int deviceId, double inletPressure, double outletPressure, double dewPoint) {
    AlarmEvent event;

    double pressureDiff = qAbs(inletPressure - outletPressure);
    if (m_evaluator.evaluate(deviceId, 1, pressureDiff, event)) {
        recordAlarmTransition(deviceId, 2, 4, 1, event,
                              QString("进出口压差超限: %1bar").arg(pressureDiff, 0, 'f', 2),
                              QString("进出口压差恢复正常: %1bar").arg(pressureDiff, 0, 'f', 2));
    }

    if (m_evaluator.evaluate(deviceId, 3, dewPoint, event)) {
        recordAlarmTransition(deviceId, 2, 5, 1, event,
                              QString("露点超限: %1°C").arg(dewPoint, 0, 'f', 1),
                              QString("露点恢复正常: %1°C").arg(dewPoint, 0, 'f', 1));
    }
}

void AlarmManager::recordAlarmTransition(int deviceId, int deviceType, int alarmType, int alarmLevel,
                                         const AlarmEvent& event, const QString& triggeredMessage,
                                         const QString& recoveredMessage) {
    const QString& message = event.isTriggered ? triggeredMessage : recoveredMessage;
    recordAlarmEvent(deviceId, deviceType, alarmType, alarmLevel,
                     event.value, event.threshold, message, event.isTriggered);
}

void AlarmManager::recordAlarmEvent(int deviceId, int deviceType, int alarmType, int alarmLevel,
                                     double value, double threshold, const QString& message, bool isTriggered) {
    if (!isTriggered) {
        emit alarmRecovered(AlarmRecord());
        return;
    }
    
    AlarmRecord record;
    record.timestamp = QDateTime::currentMSecsSinceEpoch();
    record.deviceId = deviceId;
    record.deviceType = deviceType;
    record.alarmType = alarmType;
    record.alarmLevel = alarmLevel;
    record.value = value;
    record.threshold = threshold;
    record.message = message;
    record.alarmTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    record.isTriggered = isTriggered;

    if (!m_alarmRecordWriter) {
        return;
    }

    const Application::AlarmRecordWriteStatus writeStatus =
        m_alarmRecordWriter->writeAlarmRecord(record);
    if (writeStatus == Application::AlarmRecordWriteStatus::ConnectionFailed) {
        return;
    }

    emit alarmTriggered(record);
}

void AlarmManager::updateAlarmConfig(AlarmType type, double threshold, double deadband, uint8_t debounceCycles) {
    AlarmConfigManager::instance().updateRule(type, threshold, deadband, debounceCycles);
    AlarmConfigManager::instance().applyToEvaluator(m_evaluator);
}

QList<AlarmRule> AlarmManager::getAllRules() const {
    return AlarmConfigManager::instance().getAllRules();
}

QList<AlarmRule> AlarmManager::getRulesForDevice(int deviceId) const {
    return AlarmConfigManager::instance().getRulesForDevice(deviceId);
}

}
