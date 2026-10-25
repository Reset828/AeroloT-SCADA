#include <QtTest>

#include "AlarmEngine/alarm_config_manager.h"

class AlarmConfigManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void defaultRulesCoverAllConfiguredDevices();
    void updatingRuleChangesOnlyItsStoredKey();
    void initializationRestoresDefaultRules();
    void applyToEvaluatorUsesCurrentPointMapping();
};

void AlarmConfigManagerTests::init()
{
    AlarmEngine::AlarmConfigManager::instance().initialize();
}

void AlarmConfigManagerTests::defaultRulesCoverAllConfiguredDevices()
{
    auto& manager = AlarmEngine::AlarmConfigManager::instance();

    QCOMPARE(manager.getAllRules().size(), 14);
    QCOMPARE(manager.getRulesForDevice(1).size(), 3);
    QCOMPARE(manager.getRulesForDevice(2).size(), 3);
    QCOMPARE(manager.getRulesForDevice(3).size(), 3);
    QCOMPARE(manager.getRulesForDevice(4).size(), 2);
    QCOMPARE(manager.getRulesForDevice(5).size(), 3);
    QVERIFY(manager.getRulesForDevice(6).isEmpty());

    const auto temperature = manager.getRule(AlarmEngine::AlarmType::CompressorTempHigh);
    QCOMPARE(temperature.deviceId, 1);
    QCOMPARE(temperature.pointId, 1);
    QCOMPARE(temperature.threshold, 80.0);
    QCOMPARE(temperature.deadband, 2.0);
    QCOMPARE(temperature.debounceCycles, uint8_t(3));
    QVERIFY(temperature.isHighAlarm);
}

void AlarmConfigManagerTests::updatingRuleChangesOnlyItsStoredKey()
{
    auto& manager = AlarmEngine::AlarmConfigManager::instance();
    manager.updateRule(AlarmEngine::AlarmType::CompressorTempHigh, 81.5, 2.5, 4);

    const auto firstCompressor = manager.getRule(AlarmEngine::AlarmType::CompressorTempHigh);
    QCOMPARE(firstCompressor.threshold, 81.5);
    QCOMPARE(firstCompressor.deadband, 2.5);
    QCOMPARE(firstCompressor.debounceCycles, uint8_t(4));

    bool foundDeviceTwoTemperature = false;
    for (const auto& rule : manager.getRulesForDevice(2)) {
        if (rule.pointId == 1) {
            foundDeviceTwoTemperature = true;
            QCOMPARE(rule.threshold, 80.0);
            QCOMPARE(rule.deadband, 2.0);
            QCOMPARE(rule.debounceCycles, uint8_t(3));
        }
    }
    QVERIFY(foundDeviceTwoTemperature);
}

void AlarmConfigManagerTests::initializationRestoresDefaultRules()
{
    auto& manager = AlarmEngine::AlarmConfigManager::instance();
    manager.updateRule(AlarmEngine::AlarmType::TankPressureHigh, 12.0, 1.0, 1);
    manager.initialize();

    const auto tankPressure = manager.getRule(AlarmEngine::AlarmType::TankPressureHigh);
    QCOMPARE(tankPressure.threshold, 10.0);
    QCOMPARE(tankPressure.deadband, 0.5);
    QCOMPARE(tankPressure.debounceCycles, uint8_t(3));
}

void AlarmConfigManagerTests::applyToEvaluatorUsesCurrentPointMapping()
{
    auto& manager = AlarmEngine::AlarmConfigManager::instance();
    AlarmEngine::AlarmEvaluator evaluator;
    manager.applyToEvaluator(evaluator);
    AlarmEngine::AlarmEvent event{};

    QVERIFY(!evaluator.evaluate(2, 1, 80.1, event));
    QVERIFY(!evaluator.evaluate(2, 1, 80.2, event));
    QVERIFY(evaluator.evaluate(2, 1, 80.3, event));
    QCOMPARE(event.deviceId, 2);
    QCOMPARE(event.pointId, 1);
    QCOMPARE(event.threshold, 80.0);

    QVERIFY(!evaluator.evaluate(5, 3, -41.0, event));
    QVERIFY(!evaluator.evaluate(5, 3, -42.0, event));
    QVERIFY(evaluator.evaluate(5, 3, -43.0, event));
    QCOMPARE(event.threshold, -40.0);
    QVERIFY(event.isTriggered);
}

QTEST_GUILESS_MAIN(AlarmConfigManagerTests)

#include "alarm_config_manager_tests.moc"
