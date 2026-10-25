#include <QtTest>

#include "AlarmEngine/alarm_evaluator.h"

namespace {

AlarmEngine::AlarmConfig highAlarmConfig(int debounceCycles = 3)
{
    return {
        7,
        2,
        core::AlarmSeverity::Critical,
        100.0,
        10.0,
        static_cast<uint8_t>(debounceCycles),
        true
    };
}

AlarmEngine::AlarmConfig lowAlarmConfig(int debounceCycles = 3)
{
    return {
        7,
        3,
        core::AlarmSeverity::Warning,
        -40.0,
        2.0,
        static_cast<uint8_t>(debounceCycles),
        false
    };
}

}

class AlarmEvaluatorTests : public QObject
{
    Q_OBJECT

private slots:
    void highAlarmRequiresConsecutiveSamples();
    void highAlarmThresholdEqualityResetsDebounce();
    void normalSampleResetsHighAlarmDebounce();
    void highAlarmUsesHysteresisAndRecoversOnOneSample();
    void lowAlarmThresholdEqualityResetsDebounce();
    void lowAlarmRequiresConsecutiveSamplesAndRecoversOnOneSample();
    void recoveryThresholdEqualityRetainsAlarm();
    void batchEvaluationUsesDeviceOneAndEmitsOnlyTransitions();
    void resetClearsAlarmAndDebounceState();
    void missingConfigurationOnlyClearsTriggeredOutput();
    void removingConfigurationRetainsDebounceState();
};

void AlarmEvaluatorTests::highAlarmRequiresConsecutiveSamples()
{
    AlarmEngine::AlarmEvaluator evaluator;
    evaluator.addAlarmConfig(highAlarmConfig());
    AlarmEngine::AlarmEvent event{};

    QVERIFY(!evaluator.evaluate(7, 2, 101.0, event));
    QVERIFY(!event.isTriggered);
    QVERIFY(!evaluator.evaluate(7, 2, 102.0, event));
    QVERIFY(!event.isTriggered);
    QVERIFY(evaluator.evaluate(7, 2, 103.0, event));

    QVERIFY(event.isTriggered);
    QCOMPARE(event.deviceId, 7);
    QCOMPARE(event.pointId, 2);
    QCOMPARE(event.threshold, 100.0);
}

void AlarmEvaluatorTests::highAlarmThresholdEqualityResetsDebounce()
{
    AlarmEngine::AlarmEvaluator evaluator;
    evaluator.addAlarmConfig(highAlarmConfig(2));
    AlarmEngine::AlarmEvent event{};

    QVERIFY(!evaluator.evaluate(7, 2, 101.0, event));
    QVERIFY(!evaluator.evaluate(7, 2, 100.0, event));
    QVERIFY(!event.isTriggered);
    QVERIFY(!evaluator.evaluate(7, 2, 101.0, event));
    QVERIFY(!event.isTriggered);
    QVERIFY(evaluator.evaluate(7, 2, 102.0, event));
    QVERIFY(event.isTriggered);
}

void AlarmEvaluatorTests::normalSampleResetsHighAlarmDebounce()
{
    AlarmEngine::AlarmEvaluator evaluator;
    evaluator.addAlarmConfig(highAlarmConfig());
    AlarmEngine::AlarmEvent event{};

    QVERIFY(!evaluator.evaluate(7, 2, 101.0, event));
    QVERIFY(!evaluator.evaluate(7, 2, 99.0, event));
    QVERIFY(!evaluator.evaluate(7, 2, 101.0, event));
    QVERIFY(!evaluator.evaluate(7, 2, 99.0, event));
    QVERIFY(!evaluator.evaluate(7, 2, 101.0, event));
    QVERIFY(!event.isTriggered);
    QVERIFY(!evaluator.evaluate(7, 2, 102.0, event));
    QVERIFY(!event.isTriggered);
    QVERIFY(evaluator.evaluate(7, 2, 103.0, event));
    QVERIFY(event.isTriggered);
}

void AlarmEvaluatorTests::highAlarmUsesHysteresisAndRecoversOnOneSample()
{
    AlarmEngine::AlarmEvaluator evaluator;
    evaluator.addAlarmConfig(highAlarmConfig());
    AlarmEngine::AlarmEvent event{};

    evaluator.evaluate(7, 2, 101.0, event);
    evaluator.evaluate(7, 2, 102.0, event);
    QVERIFY(evaluator.evaluate(7, 2, 103.0, event));
    QVERIFY(event.isTriggered);

    QVERIFY(!evaluator.evaluate(7, 2, 90.0, event));
    QVERIFY(event.isTriggered);
    QCOMPARE(event.threshold, 90.0);

    QVERIFY(!evaluator.evaluate(7, 2, 95.0, event));
    QVERIFY(event.isTriggered);
    QCOMPARE(event.threshold, 90.0);

    QVERIFY(evaluator.evaluate(7, 2, 89.0, event));
    QVERIFY(!event.isTriggered);
    QCOMPARE(event.threshold, 100.0);
}

void AlarmEvaluatorTests::lowAlarmThresholdEqualityResetsDebounce()
{
    AlarmEngine::AlarmEvaluator evaluator;
    evaluator.addAlarmConfig(lowAlarmConfig(2));
    AlarmEngine::AlarmEvent event{};

    QVERIFY(!evaluator.evaluate(7, 3, -41.0, event));
    QVERIFY(!evaluator.evaluate(7, 3, -40.0, event));
    QVERIFY(!event.isTriggered);
    QVERIFY(!evaluator.evaluate(7, 3, -41.0, event));
    QVERIFY(!event.isTriggered);
    QVERIFY(evaluator.evaluate(7, 3, -42.0, event));
    QVERIFY(event.isTriggered);
}

void AlarmEvaluatorTests::lowAlarmRequiresConsecutiveSamplesAndRecoversOnOneSample()
{
    AlarmEngine::AlarmEvaluator evaluator;
    evaluator.addAlarmConfig(lowAlarmConfig());
    AlarmEngine::AlarmEvent event{};

    QVERIFY(!evaluator.evaluate(7, 3, -41.0, event));
    QVERIFY(!evaluator.evaluate(7, 3, -42.0, event));
    QVERIFY(evaluator.evaluate(7, 3, -43.0, event));
    QVERIFY(event.isTriggered);

    QVERIFY(!evaluator.evaluate(7, 3, -38.0, event));
    QVERIFY(event.isTriggered);
    QCOMPARE(event.threshold, -38.0);

    QVERIFY(!evaluator.evaluate(7, 3, -39.0, event));
    QVERIFY(event.isTriggered);
    QCOMPARE(event.threshold, -38.0);

    QVERIFY(evaluator.evaluate(7, 3, -37.0, event));
    QVERIFY(!event.isTriggered);
    QCOMPARE(event.threshold, -40.0);
}

void AlarmEvaluatorTests::resetClearsAlarmAndDebounceState()
{
    AlarmEngine::AlarmEvaluator evaluator;
    evaluator.addAlarmConfig(highAlarmConfig());
    AlarmEngine::AlarmEvent event{};

    QVERIFY(!evaluator.evaluate(7, 2, 101.0, event));
    QVERIFY(!evaluator.evaluate(7, 2, 102.0, event));
    evaluator.reset();

    QVERIFY(!evaluator.evaluate(7, 2, 103.0, event));
    QVERIFY(!event.isTriggered);
    QVERIFY(!evaluator.evaluate(7, 2, 104.0, event));
    QVERIFY(!event.isTriggered);
    QVERIFY(evaluator.evaluate(7, 2, 105.0, event));
    QVERIFY(event.isTriggered);

    evaluator.reset();
    QVERIFY(!evaluator.evaluate(7, 2, 105.0, event));
    QVERIFY(!event.isTriggered);
}

void AlarmEvaluatorTests::recoveryThresholdEqualityRetainsAlarm()
{
    AlarmEngine::AlarmEvaluator highEvaluator;
    highEvaluator.addAlarmConfig(highAlarmConfig(1));
    AlarmEngine::AlarmEvent event{};

    QVERIFY(highEvaluator.evaluate(7, 2, 101.0, event));
    QVERIFY(event.isTriggered);
    QVERIFY(!highEvaluator.evaluate(7, 2, 90.0, event));
    QVERIFY(event.isTriggered);
    QVERIFY(highEvaluator.evaluate(7, 2, 89.0, event));
    QVERIFY(!event.isTriggered);

    AlarmEngine::AlarmEvaluator lowEvaluator;
    lowEvaluator.addAlarmConfig(lowAlarmConfig(1));

    QVERIFY(lowEvaluator.evaluate(7, 3, -41.0, event));
    QVERIFY(event.isTriggered);
    QVERIFY(!lowEvaluator.evaluate(7, 3, -38.0, event));
    QVERIFY(event.isTriggered);
    QVERIFY(lowEvaluator.evaluate(7, 3, -37.0, event));
    QVERIFY(!event.isTriggered);
}

void AlarmEvaluatorTests::missingConfigurationOnlyClearsTriggeredOutput()
{
    AlarmEngine::AlarmEvaluator evaluator;
    AlarmEngine::AlarmEvent event{};
    event.timestamp = 123;
    event.deviceId = 4;
    event.pointId = 5;
    event.severity = core::AlarmSeverity::Critical;
    event.value = 6.0;
    event.threshold = 7.0;
    event.isTriggered = true;

    QVERIFY(!evaluator.evaluate(4, 5, 8.0, event));

    QVERIFY(!event.isTriggered);
    QCOMPARE(event.timestamp, int64_t(123));
    QCOMPARE(event.deviceId, 4);
    QCOMPARE(event.pointId, 5);
    QCOMPARE(static_cast<int>(event.severity), static_cast<int>(core::AlarmSeverity::Critical));
    QCOMPARE(event.value, 6.0);
    QCOMPARE(event.threshold, 7.0);
    QCOMPARE(evaluator.getLastEvaluateTimeUs(), int64_t(0));
}

void AlarmEvaluatorTests::removingConfigurationRetainsDebounceState()
{
    AlarmEngine::AlarmEvaluator evaluator;
    const auto config = highAlarmConfig(3);
    evaluator.addAlarmConfig(config);
    AlarmEngine::AlarmEvent event{};

    QVERIFY(!evaluator.evaluate(7, 2, 101.0, event));
    QVERIFY(!evaluator.evaluate(7, 2, 102.0, event));
    evaluator.removeAlarmConfig(7, 2);
    QVERIFY(!evaluator.evaluate(7, 2, 103.0, event));
    QVERIFY(!event.isTriggered);

    evaluator.addAlarmConfig(config);
    QVERIFY(evaluator.evaluate(7, 2, 104.0, event));
    QVERIFY(event.isTriggered);
}

void AlarmEvaluatorTests::batchEvaluationUsesDeviceOneAndEmitsOnlyTransitions()
{
    AlarmEngine::AlarmEvaluator evaluator;
    auto deviceOneConfig = highAlarmConfig(2);
    deviceOneConfig.deviceId = 1;
    evaluator.addAlarmConfig(deviceOneConfig);
    evaluator.addAlarmConfig(highAlarmConfig(2));

    std::vector<AlarmEngine::AlarmEvent> events;
    evaluator.evaluateBatch({{2, 101.0}}, events);
    QVERIFY(events.empty());

    AlarmEngine::AlarmEvent event{};
    QVERIFY(!evaluator.evaluate(7, 2, 101.0, event));

    evaluator.evaluateBatch({{2, 102.0}}, events);
    QCOMPARE(events.size(), size_t(1));
    QCOMPARE(events.front().deviceId, 1);
    QVERIFY(events.front().isTriggered);

    evaluator.evaluateBatch({{2, 103.0}}, events);
    QVERIFY(events.empty());
}

QTEST_GUILESS_MAIN(AlarmEvaluatorTests)

#include "alarm_evaluator_tests.moc"
