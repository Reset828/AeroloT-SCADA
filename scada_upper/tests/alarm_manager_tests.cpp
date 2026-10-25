#include <QtTest>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>

#include "AlarmEngine/alarm_manager.h"
#include "Application/alarm_record_writer.h"
#include "DataStore/database_manager.h"

class AlarmManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void triggerPersistsRecordAndRecoveryEmitsCurrentEmptyRecord();

private:
    QTemporaryDir m_temporaryDirectory;
};

void AlarmManagerTests::initTestCase()
{
    QVERIFY(m_temporaryDirectory.isValid());
    QVERIFY(DataStore::DatabaseManager::instance().initialize(
        m_temporaryDirectory.filePath(QStringLiteral("alarm-test.sqlite"))));
}

void AlarmManagerTests::cleanupTestCase()
{
    const QString connectionName = QStringLiteral("alarm_insert_%1")
        .arg(quintptr(QThread::currentThreadId()));
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase::database(connectionName).close();
        QSqlDatabase::removeDatabase(connectionName);
    }
    DataStore::DatabaseManager::instance().close();
}

void AlarmManagerTests::triggerPersistsRecordAndRecoveryEmitsCurrentEmptyRecord()
{
    auto& manager = AlarmEngine::AlarmManager::instance();
    manager.setAlarmRecordWriter(DataStore::DatabaseManager::instance());
    manager.initialize();
    manager.updateAlarmConfig(AlarmEngine::AlarmType::CompressorTempHigh,
                              80.0, 2.0, 1);

    int triggerCount = 0;
    int recoveryCount = 0;
    AlarmEngine::AlarmRecord triggeredRecord{};
    AlarmEngine::AlarmRecord recoveredRecord{};
    connect(&manager, &AlarmEngine::AlarmManager::alarmTriggered,
            this, [&](const AlarmEngine::AlarmRecord& record) {
        ++triggerCount;
        triggeredRecord = record;
    });
    connect(&manager, &AlarmEngine::AlarmManager::alarmRecovered,
            this, [&](const AlarmEngine::AlarmRecord& record) {
        ++recoveryCount;
        recoveredRecord = record;
    });

    manager.processDeviceData(1, static_cast<int>(core::DeviceType::Compressor),
                              5.0, 81.0, 10.0, 5.0, 0.0);

    QCOMPARE(triggerCount, 1);
    QCOMPARE(triggeredRecord.deviceId, 1);
    QCOMPARE(triggeredRecord.deviceType, 0);
    QCOMPARE(triggeredRecord.alarmType, 2);
    QCOMPARE(triggeredRecord.alarmLevel, 2);
    QCOMPARE(triggeredRecord.value, 81.0);
    QCOMPARE(triggeredRecord.threshold, 80.0);
    QCOMPARE(triggeredRecord.message, QStringLiteral("温度超限: 81.0°C"));
    QVERIFY(triggeredRecord.isTriggered);
    QVERIFY(triggeredRecord.timestamp > 0);

    QSqlQuery persisted(DataStore::DatabaseManager::instance().getDatabase());
    QVERIFY(persisted.exec(QStringLiteral(
        "SELECT timestamp, device_id, device_type, alarm_type, alarm_level, value, threshold, message "
        "FROM alarm_log")));
    QVERIFY(persisted.next());
    QVERIFY(persisted.value(0).toLongLong() > 0);
    QCOMPARE(persisted.value(1).toInt(), 1);
    QCOMPARE(persisted.value(2).toInt(), 0);
    QCOMPARE(persisted.value(3).toInt(), 2);
    QCOMPARE(persisted.value(4).toInt(), 2);
    QCOMPARE(persisted.value(5).toDouble(), 81.0);
    QCOMPARE(persisted.value(6).toDouble(), 80.0);
    QCOMPARE(persisted.value(7).toString(), QStringLiteral("温度超限: 81.0°C"));
    QVERIFY(!persisted.next());
    persisted.finish();

    manager.processDeviceData(1, static_cast<int>(core::DeviceType::Compressor),
                              5.0, 77.0, 10.0, 5.0, 0.0);

    QCOMPARE(recoveryCount, 1);
    QCOMPARE(recoveredRecord.deviceId, 0);
    QCOMPARE(recoveredRecord.deviceType, 0);
    QCOMPARE(recoveredRecord.alarmType, 0);
    QCOMPARE(recoveredRecord.value, 0.0);
    QVERIFY(recoveredRecord.message.isEmpty());
    QVERIFY(!recoveredRecord.isTriggered);

    QSqlQuery countQuery(DataStore::DatabaseManager::instance().getDatabase());
    QVERIFY(countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM alarm_log")));
    QVERIFY(countQuery.next());
    QCOMPARE(countQuery.value(0).toInt(), 1);
    countQuery.finish();

    QSqlQuery rejectInsert(DataStore::DatabaseManager::instance().getDatabase());
    QVERIFY(rejectInsert.exec(QStringLiteral(
        "CREATE TRIGGER reject_alarm_insert BEFORE INSERT ON alarm_log "
        "BEGIN SELECT RAISE(FAIL, 'blocked'); END")));
    rejectInsert.finish();

    manager.processDeviceData(1, static_cast<int>(core::DeviceType::Compressor),
                              5.0, 81.0, 10.0, 5.0, 0.0);

    QCOMPARE(triggerCount, 2);
    QSqlQuery countAfterRejectedInsert(DataStore::DatabaseManager::instance().getDatabase());
    QVERIFY(countAfterRejectedInsert.exec(QStringLiteral("SELECT COUNT(*) FROM alarm_log")));
    QVERIFY(countAfterRejectedInsert.next());
    QCOMPARE(countAfterRejectedInsert.value(0).toInt(), 1);
    countAfterRejectedInsert.finish();

    QSqlQuery removeTrigger(DataStore::DatabaseManager::instance().getDatabase());
    QVERIFY(removeTrigger.exec(QStringLiteral("DROP TRIGGER reject_alarm_insert")));
}

QTEST_GUILESS_MAIN(AlarmManagerTests)

#include "alarm_manager_tests.moc"
