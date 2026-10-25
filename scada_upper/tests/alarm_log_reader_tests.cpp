#include <QtTest>

#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>

#include "DataStore/alarm_log_reader.h"
#include "DataStore/database_manager.h"

class AlarmLogReaderTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void filtersInclusiveRangeAndSortsNewestFirst();

private:
    void insertRecord(qint64 timestamp, int deviceId, const QString& message);

    QTemporaryDir m_temporaryDirectory;
};

void AlarmLogReaderTests::initTestCase()
{
    QVERIFY(m_temporaryDirectory.isValid());
    QVERIFY(DataStore::DatabaseManager::instance().initialize(
        m_temporaryDirectory.filePath(QStringLiteral("alarm-log-reader.sqlite"))));

    insertRecord(100, 1, QStringLiteral("before"));
    insertRecord(200, 2, QStringLiteral("start boundary"));
    insertRecord(300, 3, QStringLiteral("end boundary"));
    insertRecord(400, 4, QStringLiteral("after"));
}

void AlarmLogReaderTests::cleanupTestCase()
{
    DataStore::AlarmLogReader::closeCurrentThreadConnection();
    DataStore::DatabaseManager::instance().close();
}

void AlarmLogReaderTests::insertRecord(qint64 timestamp, int deviceId,
                                       const QString& message)
{
    QSqlQuery query(DataStore::DatabaseManager::instance().getDatabase());
    query.prepare(QStringLiteral(
        "INSERT INTO alarm_log (timestamp, device_id, device_type, alarm_type, "
        "alarm_level, value, threshold, message, alarm_time) "
        "VALUES (?, ?, 0, 2, 1, 42.5, 40.0, ?, ?)"));
    query.addBindValue(timestamp);
    query.addBindValue(deviceId);
    query.addBindValue(message);
    query.addBindValue(QString::number(timestamp));
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}

void AlarmLogReaderTests::filtersInclusiveRangeAndSortsNewestFirst()
{
    const auto result = DataStore::AlarmLogReader::read(
        DataStore::DatabaseManager::instance().databasePath(), 200, 300);

    QCOMPARE(result.failure, DataStore::AlarmLogReadFailure::None);
    QCOMPARE(result.records.size(), 2);
    QCOMPARE(result.records.at(0).deviceId, 3);
    QCOMPARE(result.records.at(0).alarmTime, QStringLiteral("300"));
    QCOMPARE(result.records.at(0).alarmType, 2);
    QCOMPARE(result.records.at(0).message, QStringLiteral("end boundary"));
    QCOMPARE(result.records.at(0).alarmLevel, 1);
    QCOMPARE(result.records.at(1).deviceId, 2);
    QCOMPARE(result.records.at(1).alarmTime, QStringLiteral("200"));
    QCOMPARE(result.records.at(1).message, QStringLiteral("start boundary"));
}

QTEST_GUILESS_MAIN(AlarmLogReaderTests)

#include "alarm_log_reader_tests.moc"
