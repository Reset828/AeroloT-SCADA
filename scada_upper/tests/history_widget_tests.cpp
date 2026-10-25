#include <QtTest>

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QThread>

#include "DataStore/database_manager.h"
#include "DataStore/history_data_reader.h"
#include "UI/history_widget.h"

class HistoryWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void loadsDeviceSpecificSeriesAndSamplesLargeRanges();
    void emptyRangeReturnsNoPoints();
    void trendQueriesPreserveColumnsAndBatchOrder();

private:
    void insertHistoryRow(int deviceId, qint64 timestamp, double pressure,
                          double temperature, double current,
                          double outletPressure, double dewPoint);

    QTemporaryDir m_temporaryDirectory;
    QThread m_workerThread;
    UI::HistoryDataWorker* m_worker{nullptr};
};

void HistoryWidgetTests::initTestCase()
{
    QVERIFY(m_temporaryDirectory.isValid());
    QVERIFY(DataStore::DatabaseManager::instance().initialize(
        m_temporaryDirectory.filePath(QStringLiteral("history-test.sqlite"))));

    for (int row = 0; row < 5; ++row) {
        const double value = row + 1;
        insertHistoryRow(1, 1000 + row * 1000,
                         10.0 + value, 20.0 + value, 30.0 + value,
                         40.0 + value, 50.0 + value);
    }
    insertHistoryRow(4, 6000, 14.0, 24.0, 0.0, 44.0, 0.0);
    insertHistoryRow(5, 7000, 15.0, 25.0, 0.0, 45.0, 55.0);

    m_worker = new UI::HistoryDataWorker();
    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    m_workerThread.start();
}

void HistoryWidgetTests::cleanupTestCase()
{
    if (m_workerThread.isRunning()) {
        m_workerThread.quit();
        m_workerThread.wait();
    }
    m_worker = nullptr;
    DataStore::HistoryDataReader::closeCurrentThreadConnections();
    DataStore::DatabaseManager::instance().close();
}

void HistoryWidgetTests::insertHistoryRow(int deviceId, qint64 timestamp,
                                         double pressure, double temperature,
                                         double current, double outletPressure,
                                         double dewPoint)
{
    QSqlQuery query(DataStore::DatabaseManager::instance().getDatabase());
    query.prepare(QStringLiteral(
        "INSERT INTO history_data (timestamp, history_time, device_id, device_type, "
        "pressure, temperature, status, runtime_hours, vfd_frequency, phase_current, "
        "alarm_code, outlet_pressure, dew_point, regen_status) "
        "VALUES (?, ?, ?, 0, ?, ?, 0, 0, 0, ?, 0, ?, ?, 0)"));
    query.addBindValue(timestamp);
    query.addBindValue(QDateTime::fromMSecsSinceEpoch(timestamp)
                           .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    query.addBindValue(deviceId);
    query.addBindValue(pressure);
    query.addBindValue(temperature);
    query.addBindValue(current);
    query.addBindValue(outletPressure);
    query.addBindValue(dewPoint);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}

void HistoryWidgetTests::loadsDeviceSpecificSeriesAndSamplesLargeRanges()
{
    QVERIFY(m_worker);
    QVERIFY(m_worker->thread() != QThread::currentThread());
    const QString dbPath = DataStore::DatabaseManager::instance().databasePath();
    QSignalSpy dataSpy(m_worker, &UI::HistoryDataWorker::dataReady);
    QVERIFY(dataSpy.isValid());

    auto invokeLoad = [&](int deviceId, qint64 startMs, qint64 endMs,
                          const QString& deviceType, int maxPoints,
                          QList<QVariant>& result) {
        if (!QMetaObject::invokeMethod(
                m_worker, "loadData", Qt::QueuedConnection,
                Q_ARG(QString, dbPath), Q_ARG(int, deviceId),
                Q_ARG(qint64, startMs), Q_ARG(qint64, endMs),
                Q_ARG(QString, deviceType), Q_ARG(int, maxPoints))) {
            return false;
        }
        if (!dataSpy.wait(3000)) {
            return false;
        }
        result = dataSpy.takeFirst();
        return true;
    };

    QList<QVariant> result;
    QVERIFY(invokeLoad(1, 1000, 5000, QStringLiteral("COM"), 10, result));
    QCOMPARE(result.at(6).toInt(), 5);
    QCOMPARE(result.at(7).toInt(), 5);
    QCOMPARE(result.at(0).value<QVector<double>>(), QVector<double>({1, 2, 3, 4, 5}));
    QCOMPARE(result.at(1).value<QVector<double>>(), QVector<double>({21, 22, 23, 24, 25}));
    QCOMPARE(result.at(3).value<QVector<double>>(), QVector<double>({11, 12, 13, 14, 15}));
    QCOMPARE(result.at(5).value<QVector<double>>(), QVector<double>({31, 32, 33, 34, 35}));
    QCOMPARE(result.at(2).value<QVector<double>>(), result.at(0).value<QVector<double>>());
    QCOMPARE(result.at(4).value<QVector<double>>(), result.at(0).value<QVector<double>>());

    QVERIFY(invokeLoad(4, 6000, 6000, QStringLiteral("TANK"), 10, result));
    QCOMPARE(result.at(1).value<QVector<double>>(), QVector<double>({14}));
    QCOMPARE(result.at(3).value<QVector<double>>(), QVector<double>({24}));
    QCOMPARE(result.at(5).value<QVector<double>>(), QVector<double>({0}));

    QVERIFY(invokeLoad(5, 7000, 7000, QStringLiteral("DRY"), 10, result));
    QCOMPARE(result.at(1).value<QVector<double>>(), QVector<double>({15}));
    QCOMPARE(result.at(3).value<QVector<double>>(), QVector<double>({45}));
    QCOMPARE(result.at(5).value<QVector<double>>(), QVector<double>({55}));

    QVERIFY(invokeLoad(1, 1000, 5000, QStringLiteral("COM"), 2, result));
    QCOMPARE(result.at(6).toInt(), 5);
    QCOMPARE(result.at(7).toInt(), 2);
    QCOMPARE(result.at(0).value<QVector<double>>(), QVector<double>({1, 3}));
}

void HistoryWidgetTests::emptyRangeReturnsNoPoints()
{
    QVERIFY(m_worker);
    QSignalSpy dataSpy(m_worker, &UI::HistoryDataWorker::dataReady);
    QVERIFY(dataSpy.isValid());
    QVERIFY(QMetaObject::invokeMethod(
        m_worker, "loadData", Qt::QueuedConnection,
        Q_ARG(QString, DataStore::DatabaseManager::instance().databasePath()),
        Q_ARG(int, 99), Q_ARG(qint64, 0), Q_ARG(qint64, 10000),
        Q_ARG(QString, QStringLiteral("COM")), Q_ARG(int, 200)));
    QVERIFY(dataSpy.wait(3000));
    const QList<QVariant> result = dataSpy.takeFirst();
    QCOMPARE(result.at(6).toInt(), 0);
    QCOMPARE(result.at(7).toInt(), 0);
    for (int index = 0; index < 6; ++index) {
        QVERIFY(result.at(index).value<QVector<double>>().isEmpty());
    }
}

void HistoryWidgetTests::trendQueriesPreserveColumnsAndBatchOrder()
{
    const QString dbPath = DataStore::DatabaseManager::instance().databasePath();
    const auto count = DataStore::HistoryDataReader::countTrendData(
        dbPath, 1, 1000, 5000);
    QCOMPARE(count.failure, DataStore::HistoryReadFailure::None);
    QCOMPARE(count.pointCount, 5);

    const auto firstBatch = DataStore::HistoryDataReader::readTrendBatch(
        dbPath, 1, 1000, 5000, QStringLiteral("COM"), 0, 2);
    QCOMPARE(firstBatch.failure, DataStore::HistoryReadFailure::None);
    QVERIFY(!firstBatch.isLastBatch);
    QCOMPARE(firstBatch.keys, QVector<double>({1, 2}));
    QCOMPARE(firstBatch.values1, QVector<double>({21, 22}));
    QCOMPARE(firstBatch.values2, QVector<double>({11, 12}));
    QCOMPARE(firstBatch.values3, QVector<double>({31, 32}));

    const auto finalBatch = DataStore::HistoryDataReader::readTrendBatch(
        dbPath, 1, 1000, 5000, QStringLiteral("COM"), 4, 2);
    QVERIFY(finalBatch.isLastBatch);
    QCOMPARE(finalBatch.keys, QVector<double>({5}));
    QCOMPARE(finalBatch.values1, QVector<double>({25}));

    const auto tankBatch = DataStore::HistoryDataReader::readTrendBatch(
        dbPath, 4, 6000, 6000, QStringLiteral("TANK"), 0, 2);
    QCOMPARE(tankBatch.values1, QVector<double>({14}));
    QCOMPARE(tankBatch.values2, QVector<double>({24}));
    QCOMPARE(tankBatch.values3, QVector<double>({0}));

    const auto dryerBatch = DataStore::HistoryDataReader::readTrendBatch(
        dbPath, 5, 7000, 7000, QStringLiteral("DRY"), 0, 2);
    QCOMPARE(dryerBatch.values1, QVector<double>({15}));
    QCOMPARE(dryerBatch.values2, QVector<double>({45}));
    QCOMPARE(dryerBatch.values3, QVector<double>({55}));

    const auto latest = DataStore::HistoryDataReader::readLatestValues(dbPath, 1);
    QCOMPARE(latest.failure, DataStore::HistoryReadFailure::None);
    QVERIFY(latest.found);
    QCOMPARE(latest.temperature, 25.0);
    QCOMPARE(latest.pressure, 15.0);
    QCOMPARE(latest.phaseCurrent, 35.0);
}

QTEST_GUILESS_MAIN(HistoryWidgetTests)

#include "history_widget_tests.moc"
