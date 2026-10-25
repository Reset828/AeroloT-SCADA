#include <QtTest>

#include "ModbusEngine/modbus_engine.h"
#include "ModbusEngine/scan_scheduler.h"

class ScanSchedulerTests : public QObject
{
    Q_OBJECT

private slots:
    void usesFiveHundredMillisecondDefaultAndScansFiveDevices();
    void reportsConfiguredDeviceCount();
    void stopPreventsFurtherScans();
};

void ScanSchedulerTests::usesFiveHundredMillisecondDefaultAndScansFiveDevices()
{
    ModbusEngine::ModbusEngine engine;
    ModbusEngine::ScanScheduler scheduler(&engine);

    QCOMPARE(scheduler.scanInterval(), 500);

    QSignalSpy scanSpy(&scheduler, &ModbusEngine::ScanScheduler::scanTriggered);
    QVERIFY(scanSpy.isValid());

    scheduler.start();
    QTRY_COMPARE_WITH_TIMEOUT(scanSpy.count(), 1, 1500);
    QCOMPARE(scanSpy.takeFirst().at(0).toInt(), 5);
    scheduler.stop();
}

void ScanSchedulerTests::reportsConfiguredDeviceCount()
{
    ModbusEngine::ModbusEngine engine;
    ModbusEngine::ScanScheduler scheduler(&engine);
    scheduler.setOnlineDevices({2, 4});

    QSignalSpy scanSpy(&scheduler, &ModbusEngine::ScanScheduler::scanTriggered);
    QVERIFY(scanSpy.isValid());
    QVERIFY(QMetaObject::invokeMethod(&scheduler, "onScanTimeout", Qt::DirectConnection));

    QCOMPARE(scanSpy.count(), 1);
    QCOMPARE(scanSpy.takeFirst().at(0).toInt(), 2);
}

void ScanSchedulerTests::stopPreventsFurtherScans()
{
    ModbusEngine::ModbusEngine engine;
    ModbusEngine::ScanScheduler scheduler(&engine);
    scheduler.setOnlineDevices({});

    QSignalSpy scanSpy(&scheduler, &ModbusEngine::ScanScheduler::scanTriggered);
    QVERIFY(scanSpy.isValid());

    scheduler.start();
    QTRY_COMPARE_WITH_TIMEOUT(scanSpy.count(), 1, 1500);
    scheduler.stop();
    QTest::qWait(scheduler.scanInterval() + 100);
    QCOMPARE(scanSpy.count(), 1);
}

QTEST_GUILESS_MAIN(ScanSchedulerTests)

#include "scan_scheduler_tests.moc"
