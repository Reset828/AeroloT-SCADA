#include <QtTest>

#include "Application/device_data_mapper.h"

class DeviceDataMapperTests : public QObject
{
    Q_OBJECT

private slots:
    void mapsCompressorRegistersForEachSlave();
    void mapsTankRegistersAndKeepsUnmappedFieldsAtDefaults();
    void mapsDryerRegistersAndSignedDewPoint();
    void partialResponsesKeepMissingFieldsAtDefaults();
    void ignoresEmptyResponsesAndUnsupportedSlaves();
};

void DeviceDataMapperTests::mapsCompressorRegistersForEachSlave()
{
    const QVector<quint16> values{1234, 456, 2, 9999, 357, 42};

    for (int slaveId = 1; slaveId <= 3; ++slaveId) {
        const auto mapped = Application::DeviceDataMapper::fromModbusRegisters(
            slaveId, values, 123456);
        QVERIFY(mapped.has_value());
        QCOMPARE(mapped->deviceKey, QStringLiteral("COM-00%1").arg(slaveId));
        QCOMPARE(mapped->data.deviceId, slaveId);
        QCOMPARE(static_cast<int>(mapped->data.deviceType),
                 static_cast<int>(core::DeviceType::Compressor));
        QCOMPARE(mapped->data.timestamp, int64_t(123456));
        QCOMPARE(mapped->data.pressure, 12.34);
        QCOMPARE(mapped->data.temperature, 45.6);
        QCOMPARE(mapped->data.status, 2);
        QCOMPARE(mapped->data.phaseCurrent, 35.7);
        QCOMPARE(mapped->data.alarmCode, 42);
    }
}

void DeviceDataMapperTests::mapsTankRegistersAndKeepsUnmappedFieldsAtDefaults()
{
    const auto mapped = Application::DeviceDataMapper::fromModbusRegisters(
        4, {720, 220, 1}, 200);

    QVERIFY(mapped.has_value());
    QCOMPARE(mapped->deviceKey, QStringLiteral("TANK-001"));
    QCOMPARE(static_cast<int>(mapped->data.deviceType),
             static_cast<int>(core::DeviceType::Tank));
    QCOMPARE(mapped->data.pressure, 7.2);
    QCOMPARE(mapped->data.temperature, 22.0);
    QCOMPARE(mapped->data.status, 1);
    QCOMPARE(mapped->data.phaseCurrent, 0.0);
    QCOMPARE(mapped->data.outletPressure, 0.0);
    QCOMPARE(mapped->data.dewPoint, 0.0);
}

void DeviceDataMapperTests::mapsDryerRegistersAndSignedDewPoint()
{
    const auto mapped = Application::DeviceDataMapper::fromModbusRegisters(
        5, {825, 807, 3, 65526, 4}, 300);

    QVERIFY(mapped.has_value());
    QCOMPARE(mapped->deviceKey, QStringLiteral("DRY-001"));
    QCOMPARE(static_cast<int>(mapped->data.deviceType),
             static_cast<int>(core::DeviceType::Dryer));
    QCOMPARE(mapped->data.pressure, 8.25);
    QCOMPARE(mapped->data.outletPressure, 8.07);
    QCOMPARE(mapped->data.status, 3);
    QCOMPARE(mapped->data.dewPoint, -1.0);
    QCOMPARE(mapped->data.regenStatus, 4);
}

void DeviceDataMapperTests::partialResponsesKeepMissingFieldsAtDefaults()
{
    const auto mapped = Application::DeviceDataMapper::fromModbusRegisters(
        1, {345}, 400);

    QVERIFY(mapped.has_value());
    QCOMPARE(mapped->data.pressure, 3.45);
    QCOMPARE(mapped->data.temperature, 0.0);
    QCOMPARE(mapped->data.status, 0);
    QCOMPARE(mapped->data.phaseCurrent, 0.0);
    QCOMPARE(mapped->data.alarmCode, 0);
    QCOMPARE(mapped->data.runtimeHours, 0.0);
    QCOMPARE(mapped->data.vfdFrequency, 0.0);
}

void DeviceDataMapperTests::ignoresEmptyResponsesAndUnsupportedSlaves()
{
    QVERIFY(!Application::DeviceDataMapper::fromModbusRegisters(1, {}, 500).has_value());
    QVERIFY(!Application::DeviceDataMapper::fromModbusRegisters(0, {100}, 500).has_value());
    QVERIFY(!Application::DeviceDataMapper::fromModbusRegisters(6, {100}, 500).has_value());
}

QTEST_GUILESS_MAIN(DeviceDataMapperTests)

#include "device_data_mapper_tests.moc"
