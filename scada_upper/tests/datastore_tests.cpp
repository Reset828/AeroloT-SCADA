#include <QtTest>

#include <QTemporaryDir>
#include <QSqlQuery>

#include "DataStore/datastore.h"

class DataStoreTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void walAndHistoryBatchBehavior();
    void auditBatchBehavior();
    void communicationLogsKeepFilterOrderAndLimit();

private:
    QTemporaryDir m_temporaryDirectory;
    QSignalSpy* m_historyWritten = nullptr;
};

void DataStoreTests::initTestCase()
{
    QVERIFY(m_temporaryDirectory.isValid());
    QVERIFY(DataStore::DataStore::instance().initialize(
        m_temporaryDirectory.filePath(QStringLiteral("scada-test.sqlite"))));
    m_historyWritten = new QSignalSpy(&DataStore::DataStore::instance(),
                                      &DataStore::DataStore::historyWritten);
    QVERIFY(m_historyWritten->isValid());
}

void DataStoreTests::cleanupTestCase()
{
    DataStore::DataStore::instance().shutdown();
    DataStore::DatabaseManager::instance().close();
    delete m_historyWritten;
    m_historyWritten = nullptr;
}

void DataStoreTests::walAndHistoryBatchBehavior()
{
    auto journalMode = DataStore::DatabaseManager::instance().query("PRAGMA journal_mode");
    QVERIFY(journalMode.next());
    QCOMPARE(journalMode.value(0).toString().toLower(), QStringLiteral("wal"));
    journalMode.finish();

    auto countRows = [] {
        auto query = DataStore::DatabaseManager::instance().query(
            "SELECT COUNT(*) FROM history_data");
        return query.next() ? query.value(0).toInt() : -1;
    };

    for (int i = 0; i < 99; ++i) {
        DataStore::HistoryRecord record;
        record.timestamp = i + 1;
        record.historyTime = QString::number(i + 1);
        record.deviceId = 1;
        record.pressure = 5.0 + i;
        DataStore::DataStore::instance().appendHistory(record);
    }

    QTest::qWait(150);
    QCOMPARE(m_historyWritten->count(), 0);
    QCOMPARE(countRows(), 0);

    DataStore::HistoryRecord hundredthRecord;
    hundredthRecord.timestamp = 100;
    hundredthRecord.historyTime = QStringLiteral("100");
    hundredthRecord.deviceId = 1;
    hundredthRecord.pressure = 105.0;
    DataStore::DataStore::instance().appendHistory(hundredthRecord);

    QTRY_COMPARE_WITH_TIMEOUT(m_historyWritten->count(), 1, 3000);
    QCOMPARE(m_historyWritten->takeFirst().at(0).toInt(), 100);
    QCOMPARE(countRows(), 100);

    DataStore::HistoryRecord partialRecord;
    partialRecord.timestamp = 101;
    partialRecord.historyTime = QStringLiteral("101");
    partialRecord.deviceId = 1;
    DataStore::DataStore::instance().appendHistory(partialRecord);
    DataStore::DataStore::instance().flush();

    QTRY_COMPARE_WITH_TIMEOUT(m_historyWritten->count(), 1, 3000);
    QCOMPARE(m_historyWritten->takeFirst().at(0).toInt(), 1);
    QCOMPARE(countRows(), 101);
}

void DataStoreTests::communicationLogsKeepFilterOrderAndLimit()
{
    auto& store = DataStore::DataStore::instance();
    QVERIFY(store.insertCommLog(100, QStringLiteral("Modbus"), QStringLiteral("TX"),
                                QStringLiteral("device_1"), QStringLiteral("first")));
    QVERIFY(store.insertCommLog(200, QStringLiteral("WebSocket"), QStringLiteral("WS"),
                                QStringLiteral("client"), QStringLiteral("second")));
    QVERIFY(store.insertCommLog(300, QStringLiteral("Webhook"), QStringLiteral("HTTP"),
                                QStringLiteral("alarm"), QStringLiteral("third")));

    const auto records = store.getCommLogs(100, 300, 2);
    QCOMPARE(records.size(), 2);
    QCOMPARE(records.at(0).timestamp, int64_t(300));
    QCOMPARE(records.at(0).logType, QStringLiteral("Webhook"));
    QCOMPARE(records.at(0).content, QStringLiteral("third"));
    QCOMPARE(records.at(1).timestamp, int64_t(200));
    QCOMPARE(records.at(1).direction, QStringLiteral("WS"));
}

void DataStoreTests::auditBatchBehavior()
{
    auto& store = DataStore::DataStore::instance();
    QSignalSpy auditWrittenSpy(&store, &DataStore::DataStore::auditWritten);
    QVERIFY(auditWrittenSpy.isValid());

    auto countRows = [] {
        auto query = DataStore::DatabaseManager::instance().query(
            "SELECT COUNT(*) FROM audit_log");
        return query.next() ? query.value(0).toInt() : -1;
    };

    for (int i = 0; i < 49; ++i) {
        DataStore::AuditRecord record;
        record.timestamp = 1000 + i;
        record.userName = QStringLiteral("test-user");
        record.operationType = 2;
        record.targetDeviceId = 1;
        record.targetParameter = QStringLiteral("connection_state");
        record.oldValue = QStringLiteral("disconnected");
        record.newValue = QStringLiteral("connected");
        record.result = 1;
        store.appendAudit(record);
    }

    QTest::qWait(150);
    QCOMPARE(auditWrittenSpy.count(), 0);
    QCOMPARE(countRows(), 0);

    DataStore::AuditRecord fiftiethRecord;
    fiftiethRecord.timestamp = 1049;
    fiftiethRecord.userName = QStringLiteral("test-user");
    fiftiethRecord.operationType = 2;
    fiftiethRecord.targetDeviceId = 1;
    fiftiethRecord.targetParameter = QStringLiteral("connection_state");
    fiftiethRecord.oldValue = QStringLiteral("disconnected");
    fiftiethRecord.newValue = QStringLiteral("connected");
    fiftiethRecord.result = 1;
    store.appendAudit(fiftiethRecord);

    QTRY_COMPARE_WITH_TIMEOUT(auditWrittenSpy.count(), 1, 3000);
    QCOMPARE(auditWrittenSpy.takeFirst().at(0).toInt(), 50);
    QCOMPARE(countRows(), 50);

    DataStore::AuditRecord partialRecord;
    partialRecord.timestamp = 1050;
    partialRecord.userName = QStringLiteral("test-user");
    partialRecord.operationType = 2;
    partialRecord.targetDeviceId = 1;
    partialRecord.targetParameter = QStringLiteral("connection_state");
    partialRecord.oldValue = QStringLiteral("disconnected");
    partialRecord.newValue = QStringLiteral("connected");
    partialRecord.result = 1;
    store.appendAudit(partialRecord);
    store.flush();

    QTRY_COMPARE_WITH_TIMEOUT(auditWrittenSpy.count(), 1, 3000);
    QCOMPARE(auditWrittenSpy.takeFirst().at(0).toInt(), 1);
    QCOMPARE(countRows(), 51);
}

QTEST_GUILESS_MAIN(DataStoreTests)

#include "datastore_tests.moc"
