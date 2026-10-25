#include <QtTest>

#include "ModbusEngine/register_map.h"

class RegisterMapTests : public QObject
{
    Q_OBJECT

private slots:
    void mapsCompressorRegisterNamesForAllThreeSlaves();
    void mapsTankAndDryerRegisterNames();
    void buildsReadGroupsForConfiguredDevices();
    void unsupportedSlaveUsesCompressorMapWithRequestedSlaveId();
};

void RegisterMapTests::mapsCompressorRegisterNamesForAllThreeSlaves()
{
    const QStringList expectedNames{
        QStringLiteral("discharge_pressure"),
        QStringLiteral("discharge_temp"),
        QStringLiteral("run_status"),
        QStringLiteral("load_unload"),
        QStringLiteral("phase_current"),
        QStringLiteral("alarm_code")
    };

    for (int slaveId = 1; slaveId <= 3; ++slaveId) {
        const auto namesByBase = ModbusEngine::RegisterMap::getRegisterNames(slaveId);
        QCOMPARE(namesByBase.size(), 1);
        const auto names = namesByBase.value(0);
        QCOMPARE(names.size(), expectedNames.size());
        for (int address = 0; address < expectedNames.size(); ++address)
            QCOMPARE(names.value(address), expectedNames.at(address));
    }
}

void RegisterMapTests::mapsTankAndDryerRegisterNames()
{
    const auto tank = ModbusEngine::RegisterMap::getRegisterNames(4).value(0);
    QCOMPARE(tank.size(), 3);
    QCOMPARE(tank.value(0), QStringLiteral("tank_pressure"));
    QCOMPARE(tank.value(1), QStringLiteral("tank_temp"));
    QCOMPARE(tank.value(2), QStringLiteral("tank_status"));

    const auto dryer = ModbusEngine::RegisterMap::getRegisterNames(5).value(0);
    QCOMPARE(dryer.size(), 5);
    QCOMPARE(dryer.value(0), QStringLiteral("inlet_pressure"));
    QCOMPARE(dryer.value(1), QStringLiteral("outlet_pressure"));
    QCOMPARE(dryer.value(2), QStringLiteral("run_status"));
    QCOMPARE(dryer.value(3), QStringLiteral("dew_point"));
    QCOMPARE(dryer.value(4), QStringLiteral("regen_status"));
}

void RegisterMapTests::buildsReadGroupsForConfiguredDevices()
{
    const QVector<int> expectedQuantities{6, 6, 6, 3, 5};

    for (int slaveId = 1; slaveId <= 5; ++slaveId) {
        const auto groups = ModbusEngine::RegisterMap::buildReadGroups(slaveId);
        QCOMPARE(groups.size(), size_t(1));
        QCOMPARE(groups.front().slaveId, slaveId);
        QCOMPARE(groups.front().startAddress, 0);
        QCOMPARE(groups.front().quantity, expectedQuantities.at(slaveId - 1));
        QCOMPARE(static_cast<int>(groups.front().registerType),
                 static_cast<int>(core::ModbusRegisterType::Holding));
    }
}

void RegisterMapTests::unsupportedSlaveUsesCompressorMapWithRequestedSlaveId()
{
    const auto groups = ModbusEngine::RegisterMap::buildReadGroups(99);

    QCOMPARE(groups.size(), size_t(1));
    QCOMPARE(groups.front().slaveId, 99);
    QCOMPARE(groups.front().quantity, 6);
    QCOMPARE(ModbusEngine::RegisterMap::getRegisterNames(99),
             ModbusEngine::RegisterMap::getRegisterNames(1));
}

QTEST_GUILESS_MAIN(RegisterMapTests)

#include "register_map_tests.moc"
