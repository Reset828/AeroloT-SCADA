#ifndef ALARMENGINE_ALARM_CONFIG_MANAGER_H
#define ALARMENGINE_ALARM_CONFIG_MANAGER_H

#include <QMap>
#include <QMutex>
#include <QString>
#include "alarm_evaluator.h"

namespace AlarmEngine {

enum class AlarmType {
    CompressorTempHigh,
    CompressorPressureHigh,
    CompressorCurrentHigh,
    TankPressureHigh,
    TankTempHigh,
    DryerPressureDiffHigh,
    DryerDewPointHigh,
    DryerDewPointLow
};

struct AlarmRule {
    AlarmType type;
    int deviceId;
    int pointId;
    core::AlarmSeverity severity;
    double threshold;
    double deadband;
    uint8_t debounceCycles;
    bool isHighAlarm;
    QString name;
    QString description;
};

class AlarmConfigManager {
public:
    static AlarmConfigManager& instance();

    void initialize();
    void applyToEvaluator(AlarmEvaluator& evaluator);
    AlarmRule getRule(AlarmType type) const;
    void updateRule(AlarmType type, double threshold, double deadband, uint8_t debounceCycles);
    QList<AlarmRule> getAllRules() const;
    QList<AlarmRule> getRulesForDevice(int deviceId) const;

private:
    AlarmConfigManager();
    void buildDefaultRules();
    void addCompressorRules();
    void addTankRules();
    void addDryerRules();

    QMap<AlarmType, AlarmRule> m_rules;
    mutable QMutex m_mutex;
};

}

#endif
