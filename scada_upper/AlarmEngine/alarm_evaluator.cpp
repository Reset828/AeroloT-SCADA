#include "alarm_evaluator.h"
#include "../core/timestamp.h"

namespace AlarmEngine {

AlarmEvaluator::AlarmEvaluator() : m_lastEvaluateUs(0) {
}

void AlarmEvaluator::addAlarmConfig(const AlarmConfig& config) {
    m_configs[config.deviceId][config.pointId] = config;
}

void AlarmEvaluator::removeAlarmConfig(int deviceId, int pointId) {
    auto deviceIt = m_configs.find(deviceId);
    if (deviceIt != m_configs.end()) {
        deviceIt->second.erase(pointId);
    }
}

bool AlarmEvaluator::evaluate(int deviceId, int pointId, double value, AlarmEvent& event) {
    auto startTime = std::chrono::high_resolution_clock::now();

    auto deviceIt = m_configs.find(deviceId);
    if (deviceIt == m_configs.end()) {
        event.isTriggered = false;
        m_lastEvaluateUs = 0;
        return false;
    }

    auto configIt = deviceIt->second.find(pointId);
    if (configIt == deviceIt->second.end()) {
        event.isTriggered = false;
        m_lastEvaluateUs = 0;
        return false;
    }

    const AlarmConfig& config = configIt->second;
    AlarmPointState* state = getOrCreateState(deviceId, pointId);

    bool stateChanged = updateAlarmState(value, config, *state);

    auto endTime = std::chrono::high_resolution_clock::now();
    m_lastEvaluateUs = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime).count();

    event.timestamp = core::Timestamp::currentMicros();
    event.deviceId = deviceId;
    event.pointId = pointId;
    event.severity = config.severity;
    event.value = value;
    event.threshold = state->currentThreshold;
    event.isTriggered = state->isAlarmed;

    return stateChanged;
}

void AlarmEvaluator::evaluateBatch(const std::map<int, double>& values, std::vector<AlarmEvent>& events) {
    events.clear();

    for (const auto& kv : values) {
        int pointId = kv.first;
        double value = kv.second;

        AlarmEvent event;
        if (evaluate(1, pointId, value, event)) {
            events.push_back(event);
        }
    }
}

void AlarmEvaluator::reset() {
    m_states.clear();
    m_lastEvaluateUs = 0;
}

AlarmPointState* AlarmEvaluator::getOrCreateState(int deviceId, int pointId) {
    return &m_states[deviceId][pointId];
}

bool AlarmEvaluator::updateAlarmState(double value, const AlarmConfig& config, AlarmPointState& state) {
    if (state.isAlarmed) {
        return updateAlarmRecovery(value, config, state);
    }

    return updateAlarmTrigger(value, config, state);
}

bool AlarmEvaluator::updateAlarmRecovery(double value, const AlarmConfig& config, AlarmPointState& state) {
    const double recoverThreshold = config.isHighAlarm
        ? config.threshold - config.deadband
        : config.threshold + config.deadband;
    state.currentThreshold = recoverThreshold;

    const bool recovered = config.isHighAlarm
        ? value < recoverThreshold
        : value > recoverThreshold;
    if (!recovered) {
        return false;
    }

    state.consecutiveCount = 0;
    state.isAlarmed = false;
    state.currentThreshold = config.threshold;
    return true;
}

bool AlarmEvaluator::updateAlarmTrigger(double value, const AlarmConfig& config, AlarmPointState& state) {
    state.currentThreshold = config.threshold;

    // Only strict crossings accumulate; equality resets the debounce count.
    const bool crossedThreshold = config.isHighAlarm
        ? value > config.threshold
        : value < config.threshold;
    if (!crossedThreshold) {
        state.consecutiveCount = 0;
        return false;
    }

    ++state.consecutiveCount;
    if (state.consecutiveCount < config.debounceCycles) {
        return false;
    }

    state.isAlarmed = true;
    state.consecutiveCount = 0;
    return true;
}

}
