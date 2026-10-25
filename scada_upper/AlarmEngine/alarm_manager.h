#ifndef ALARMENGINE_ALARM_MANAGER_H
#define ALARMENGINE_ALARM_MANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include "alarm_evaluator.h"
#include "alarm_config_manager.h"
#include "Application/alarm_record_writer.h"
#include "Application/device_data_ports.h"
#include "core/alarm_record.h"

namespace AlarmEngine {

using AlarmRecord = core::AlarmRecord;

class AlarmManager : public QObject, public Application::DeviceAlarmProcessor {
    Q_OBJECT
public:
    static AlarmManager& instance();

    void initialize();
    void setAlarmRecordWriter(Application::AlarmRecordWriter& writer);
    void processDeviceData(const core::DeviceData& data) override;
    void processDeviceData(int deviceId, int deviceType,
                           double pressure, double temperature,
                           double current, double outletPressure,
                           double dewPoint);

    void updateAlarmConfig(AlarmType type, double threshold, double deadband, uint8_t debounceCycles);
    QList<AlarmRule> getAllRules() const;
    QList<AlarmRule> getRulesForDevice(int deviceId) const;

signals:
    void alarmTriggered(const AlarmRecord& record);
    void alarmRecovered(const AlarmRecord& record);

private:
    AlarmManager();
    void evaluateCompressor(int deviceId, double temperature, double pressure, double current);
    void evaluateTank(int deviceId, double pressure, double temperature);
    void evaluateDryer(int deviceId, double inletPressure, double outletPressure, double dewPoint);
    void recordAlarmTransition(int deviceId, int deviceType, int alarmType, int alarmLevel,
                               const AlarmEvent& event, const QString& triggeredMessage,
                               const QString& recoveredMessage);
    void recordAlarmEvent(int deviceId, int deviceType, int alarmType, int alarmLevel,
                          double value, double threshold, const QString& message, bool isTriggered);

    AlarmEvaluator m_evaluator;
    Application::AlarmRecordWriter* m_alarmRecordWriter{nullptr};
};

}

#endif
