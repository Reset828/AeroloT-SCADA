#include "alarm_config_manager.h"

namespace AlarmEngine {

AlarmConfigManager::AlarmConfigManager() {
}

AlarmConfigManager& AlarmConfigManager::instance() {
    static AlarmConfigManager inst;
    return inst;
}

void AlarmConfigManager::initialize() {
    QMutexLocker locker(&m_mutex);
    buildDefaultRules();
}

void AlarmConfigManager::applyToEvaluator(AlarmEvaluator& evaluator) {
    QMutexLocker locker(&m_mutex);
    evaluator.reset();
    for (auto it = m_rules.constBegin(); it != m_rules.constEnd(); ++it) {
        const AlarmRule& rule = it.value();
        AlarmConfig config;
        config.deviceId = rule.deviceId;
        config.pointId = rule.pointId;
        config.severity = rule.severity;
        config.threshold = rule.threshold;
        config.deadband = rule.deadband;
        config.debounceCycles = rule.debounceCycles;
        config.isHighAlarm = rule.isHighAlarm;
        evaluator.addAlarmConfig(config);
    }
}

AlarmRule AlarmConfigManager::getRule(AlarmType type) const {
    QMutexLocker locker(&m_mutex);
    return m_rules.value(type);
}

void AlarmConfigManager::updateRule(AlarmType type, double threshold, double deadband, uint8_t debounceCycles) {
    QMutexLocker locker(&m_mutex);
    if (m_rules.contains(type)) {
        AlarmRule& rule = m_rules[type];
        rule.threshold = threshold;
        rule.deadband = deadband;
        rule.debounceCycles = debounceCycles;
    }
}

QList<AlarmRule> AlarmConfigManager::getAllRules() const {
    QMutexLocker locker(&m_mutex);
    return m_rules.values();
}

QList<AlarmRule> AlarmConfigManager::getRulesForDevice(int deviceId) const {
    QMutexLocker locker(&m_mutex);
    QList<AlarmRule> result;
    for (auto it = m_rules.constBegin(); it != m_rules.constEnd(); ++it) {
        if (it.value().deviceId == deviceId) {
            result.append(it.value());
        }
    }
    return result;
}

void AlarmConfigManager::buildDefaultRules() {
    m_rules.clear();
    addCompressorRules();
    addTankRules();
    addDryerRules();
}

void AlarmConfigManager::addCompressorRules() {
    AlarmRule temperatureRule;
    temperatureRule.type = AlarmType::CompressorTempHigh;
    temperatureRule.deviceId = 1;
    temperatureRule.pointId = 1;
    temperatureRule.severity = core::AlarmSeverity::Critical;
    temperatureRule.threshold = 80.0;
    temperatureRule.deadband = 2.0;
    temperatureRule.debounceCycles = 3;
    temperatureRule.isHighAlarm = true;
    temperatureRule.name = "空压机温度高报警";
    temperatureRule.description = "温度超过80°C";
    m_rules[AlarmType::CompressorTempHigh] = temperatureRule;

    AlarmRule pressureRule;
    pressureRule.type = AlarmType::CompressorPressureHigh;
    pressureRule.deviceId = 1;
    pressureRule.pointId = 2;
    pressureRule.severity = core::AlarmSeverity::Critical;
    pressureRule.threshold = 8.5;
    pressureRule.deadband = 0.3;
    pressureRule.debounceCycles = 3;
    pressureRule.isHighAlarm = true;
    pressureRule.name = "空压机排气压力高报警";
    pressureRule.description = "排气压力超过8.5bar";
    m_rules[AlarmType::CompressorPressureHigh] = pressureRule;

    AlarmRule currentRule;
    currentRule.type = AlarmType::CompressorCurrentHigh;
    currentRule.deviceId = 1;
    currentRule.pointId = 3;
    currentRule.severity = core::AlarmSeverity::Warning;
    currentRule.threshold = 20.0;
    currentRule.deadband = 1.0;
    currentRule.debounceCycles = 3;
    currentRule.isHighAlarm = true;
    currentRule.name = "空压机运行电流高报警";
    currentRule.description = "运行电流超过20A";
    m_rules[AlarmType::CompressorCurrentHigh] = currentRule;

    for (int deviceId = 2; deviceId <= 3; ++deviceId) {
        AlarmRule deviceTemperatureRule = temperatureRule;
        deviceTemperatureRule.deviceId = deviceId;
        m_rules[static_cast<AlarmType>(static_cast<int>(AlarmType::CompressorTempHigh)
                                       + (deviceId - 1) * 10)] = deviceTemperatureRule;

        AlarmRule devicePressureRule = pressureRule;
        devicePressureRule.deviceId = deviceId;
        m_rules[static_cast<AlarmType>(static_cast<int>(AlarmType::CompressorPressureHigh)
                                       + (deviceId - 1) * 10)] = devicePressureRule;

        AlarmRule deviceCurrentRule = currentRule;
        deviceCurrentRule.deviceId = deviceId;
        m_rules[static_cast<AlarmType>(static_cast<int>(AlarmType::CompressorCurrentHigh)
                                       + (deviceId - 1) * 10)] = deviceCurrentRule;
    }
}

void AlarmConfigManager::addTankRules() {
    AlarmRule pressureRule;
    pressureRule.type = AlarmType::TankPressureHigh;
    pressureRule.deviceId = 4;
    pressureRule.pointId = 1;
    pressureRule.severity = core::AlarmSeverity::Critical;
    pressureRule.threshold = 10.0;
    pressureRule.deadband = 0.5;
    pressureRule.debounceCycles = 3;
    pressureRule.isHighAlarm = true;
    pressureRule.name = "储气罐压力高报警";
    pressureRule.description = "罐压超过10bar";
    m_rules[AlarmType::TankPressureHigh] = pressureRule;

    AlarmRule temperatureRule;
    temperatureRule.type = AlarmType::TankTempHigh;
    temperatureRule.deviceId = 4;
    temperatureRule.pointId = 2;
    temperatureRule.severity = core::AlarmSeverity::Warning;
    temperatureRule.threshold = 50.0;
    temperatureRule.deadband = 2.0;
    temperatureRule.debounceCycles = 3;
    temperatureRule.isHighAlarm = true;
    temperatureRule.name = "储气罐温度高报警";
    temperatureRule.description = "温度超过50°C";
    m_rules[AlarmType::TankTempHigh] = temperatureRule;
}

void AlarmConfigManager::addDryerRules() {
    AlarmRule pressureDiffRule;
    pressureDiffRule.type = AlarmType::DryerPressureDiffHigh;
    pressureDiffRule.deviceId = 5;
    pressureDiffRule.pointId = 1;
    pressureDiffRule.severity = core::AlarmSeverity::Warning;
    pressureDiffRule.threshold = 8.0;
    pressureDiffRule.deadband = 0.5;
    pressureDiffRule.debounceCycles = 3;
    pressureDiffRule.isHighAlarm = true;
    pressureDiffRule.name = "干燥机压力差高报警";
    pressureDiffRule.description = "进出口压力差大于8bar";
    m_rules[AlarmType::DryerPressureDiffHigh] = pressureDiffRule;

    AlarmRule dewPointHighRule;
    dewPointHighRule.type = AlarmType::DryerDewPointHigh;
    dewPointHighRule.deviceId = 5;
    dewPointHighRule.pointId = 3;
    dewPointHighRule.severity = core::AlarmSeverity::Warning;
    dewPointHighRule.threshold = -20.0;
    dewPointHighRule.deadband = 2.0;
    dewPointHighRule.debounceCycles = 3;
    dewPointHighRule.isHighAlarm = true;
    dewPointHighRule.name = "干燥机露点高报警";
    dewPointHighRule.description = "露点大于-20°C";
    m_rules[AlarmType::DryerDewPointHigh] = dewPointHighRule;

    AlarmRule dewPointLowRule;
    dewPointLowRule.type = AlarmType::DryerDewPointLow;
    dewPointLowRule.deviceId = 5;
    dewPointLowRule.pointId = 3;
    dewPointLowRule.severity = core::AlarmSeverity::Info;
    dewPointLowRule.threshold = -40.0;
    dewPointLowRule.deadband = 2.0;
    dewPointLowRule.debounceCycles = 3;
    dewPointLowRule.isHighAlarm = false;
    dewPointLowRule.name = "干燥机露点低报警";
    dewPointLowRule.description = "露点低于-40°C";
    m_rules[AlarmType::DryerDewPointLow] = dewPointLowRule;
}

}
