#include <QtTest>

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>

#include "DataStore/database_manager.h"
#include "UI/trend_chart.h"

class TrendChartTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void loadsMoreThanTenThousandPointsWhileEventsContinue();

private:
    QTemporaryDir m_temporaryDirectory;
};

void TrendChartTests::initTestCase()
{
    QVERIFY(m_temporaryDirectory.isValid());
    QVERIFY(DataStore::DatabaseManager::instance().initialize(
        m_temporaryDirectory.filePath(QStringLiteral("trend-test.sqlite"))));

    QSqlDatabase db = DataStore::DatabaseManager::instance().getDatabase();
    QVERIFY(db.transaction());
    QSqlQuery query(db);
    QVERIFY(query.prepare(QStringLiteral(
        "INSERT INTO history_data (timestamp, history_time, device_id, device_type, "
        "pressure, temperature, status, runtime_hours, vfd_frequency, phase_current, "
        "alarm_code, outlet_pressure, dew_point, regen_status) "
        "VALUES (?, ?, 1, 0, ?, ?, 1, 0, 0, ?, 0, 0, 0, 0)")));

    constexpr int recordCount = 10001;
    const qint64 newestTimestamp = QDateTime::currentSecsSinceEpoch() * 1000;
    const qint64 firstTimestamp = newestTimestamp - (recordCount - 1) * 1000;
    for (int index = 0; index < recordCount; ++index) {
        const qint64 timestamp = firstTimestamp + index * 1000;
        query.bindValue(0, timestamp);
        query.bindValue(1, QDateTime::fromMSecsSinceEpoch(timestamp)
                               .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        query.bindValue(2, 5.0 + index % 10);
        query.bindValue(3, 70.0 + index % 20);
        query.bindValue(4, 10.0 + index % 5);
        if (!query.exec()) {
            QFAIL(qPrintable(query.lastError().text()));
        }
    }
    query.finish();
    QVERIFY(db.commit());
}

void TrendChartTests::cleanupTestCase()
{
    DataStore::DatabaseManager::instance().close();
}

void TrendChartTests::loadsMoreThanTenThousandPointsWhileEventsContinue()
{
    UI::TrendChart chart(QStringLiteral("COM-001"));
    QSignalSpy loadedSpy(&chart, &UI::TrendChart::dataLoaded);
    QVERIFY(loadedSpy.isValid());

    int timerTicks = 0;
    QTimer eventProbe;
    eventProbe.setInterval(1);
    connect(&eventProbe, &QTimer::timeout, this, [&] { ++timerTicks; });
    eventProbe.start();

    QVERIFY(loadedSpy.wait(30000));
    eventProbe.stop();

    QCOMPARE(loadedSpy.count(), 1);
    QCOMPARE(loadedSpy.takeFirst().at(0).toInt(), 10001);
    QVERIFY2(timerTicks > 0, "UI event loop did not process the probe timer during trend loading");
}

QTEST_MAIN(TrendChartTests)

#include "trend_chart_tests.moc"
