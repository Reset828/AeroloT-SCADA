#ifndef ALARMENGINE_ALARMEVALUATOR_H
#define ALARMENGINE_ALARMEVALUATOR_H

#include <cstdint>
#include <map>
#include <vector>
#include <chrono>
#include "../core/types.h"

namespace AlarmEngine {

// Alarm configuration structure
struct AlarmConfig {
    int deviceId;
    int pointId;
    core::AlarmSeverity severity;
    double threshold;
    double deadband;
    uint8_t debounceCycles;
    bool isHighAlarm;
};

// Alarm point state structure
struct AlarmPointState {
    uint8_t consecutiveCount;
    bool isAlarmed;
    double currentThreshold;
};

// Alarm event structure
struct AlarmEvent {
    int64_t timestamp;
    int deviceId;
    int pointId;
    core::AlarmSeverity severity;
    double value;
    double threshold;
    bool isTriggered;
};

class AlarmEvaluator {
public:
    AlarmEvaluator();

    void addAlarmConfig(const AlarmConfig& config);
    void removeAlarmConfig(int deviceId, int pointId);

    // Returns true only when the alarm state changes.
    bool evaluate(int deviceId, int pointId, double value, AlarmEvent& event);

    void evaluateBatch(const std::map<int, double>& values, std::vector<AlarmEvent>& events);

    // Get evaluation time in microseconds
    int64_t getLastEvaluateTimeUs() const { return m_lastEvaluateUs; }

    void reset();

private:
    AlarmPointState* getOrCreateState(int deviceId, int pointId);

    bool updateAlarmState(double value, const AlarmConfig& config, AlarmPointState& state);
    bool updateAlarmRecovery(double value, const AlarmConfig& config, AlarmPointState& state);
    bool updateAlarmTrigger(double value, const AlarmConfig& config, AlarmPointState& state);

    std::map<int, std::map<int, AlarmConfig>> m_configs;
    std::map<int, std::map<int, AlarmPointState>> m_states;

    int64_t m_lastEvaluateUs;
};

}
#endif
