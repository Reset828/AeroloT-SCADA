#include "history_data_reader.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

namespace DataStore {
namespace {

QString connectionNameForCurrentThread(const QString& connectionPrefix)
{
    return QStringLiteral("%1_%2").arg(connectionPrefix)
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

QSqlDatabase databaseForCurrentThread(const QString& dbPath,
                                       const QString& connectionPrefix,
                                       bool configureHistoryReader,
                                       QString& error)
{
    const QString connectionName = connectionNameForCurrentThread(connectionPrefix);
    if (QSqlDatabase::contains(connectionName)) {
        return QSqlDatabase::database(connectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        error = db.lastError().text();
        return {};
    }

    db.exec("PRAGMA journal_mode=WAL");
    db.exec("PRAGMA read_uncommitted=1");
    if (configureHistoryReader) {
        db.exec("PRAGMA cache_size=-64000");
        db.exec("PRAGMA temp_store=MEMORY");
    }
    return db;
}

QString valueColumnsForDeviceType(const QString& deviceType)
{
    if (deviceType == "COM") {
        return QStringLiteral("temperature, pressure, phase_current");
    }
    if (deviceType == "TANK") {
        return QStringLiteral("pressure, temperature, 0");
    }
    if (deviceType == "DRY") {
        return QStringLiteral("pressure, outlet_pressure, dew_point");
    }
    return QStringLiteral("temperature, pressure, phase_current");
}

void appendRow(QSqlQuery& query, HistoryDataResult& result)
{
    const double key = query.value(0).toLongLong() / 1000.0;
    result.keys1.append(key);
    result.values1.append(query.value(1).toDouble());
    result.keys2.append(key);
    result.values2.append(query.value(2).toDouble());
    result.keys3.append(key);
    result.values3.append(query.value(3).toDouble());
}

}

HistoryDataResult HistoryDataReader::read(const QString& dbPath, int deviceId,
                                          qint64 startMs, qint64 endMs,
                                          const QString& deviceType, int maxPoints)
{
    HistoryDataResult result;
    QSqlDatabase db = databaseForCurrentThread(
        dbPath, QStringLiteral("history_worker"), true, result.error);
    if (!db.isOpen() && !result.error.isEmpty()) {
        result.error = QStringLiteral("Failed to open database");
        return result;
    }

    QSqlQuery countQuery(db);
    countQuery.prepare("SELECT COUNT(*) FROM history_data WHERE device_id = ? AND timestamp >= ? AND timestamp <= ?");
    countQuery.addBindValue(deviceId);
    countQuery.addBindValue(startMs);
    countQuery.addBindValue(endMs);
    if (!countQuery.exec()) {
        result.error = QStringLiteral("Failed to count data");
        return result;
    }
    if (countQuery.next()) {
        result.pointCount = countQuery.value(0).toInt();
    }

    if (result.pointCount == 0) {
        return result;
    }

    result.sampledCount = result.pointCount;
    const QString valueColumns = valueColumnsForDeviceType(deviceType);
    const QString sql = QStringLiteral(
        "SELECT timestamp, %1 FROM history_data "
        "WHERE device_id = ? AND timestamp >= ? AND timestamp <= ? "
        "ORDER BY timestamp ASC").arg(valueColumns);

    QSqlQuery query(db);
    query.setForwardOnly(true);
    query.prepare(sql);
    query.addBindValue(deviceId);
    query.addBindValue(startMs);
    query.addBindValue(endMs);
    if (!query.exec()) {
        result.error = QStringLiteral("Failed to execute query");
        return result;
    }

    if (result.pointCount <= maxPoints) {
        result.keys1.reserve(result.pointCount);
        result.values1.reserve(result.pointCount);
        result.keys2.reserve(result.pointCount);
        result.values2.reserve(result.pointCount);
        result.keys3.reserve(result.pointCount);
        result.values3.reserve(result.pointCount);

        while (query.next()) {
            appendRow(query, result);
        }
        result.sampledCount = result.keys1.size();
        return result;
    }

    result.sampledCount = maxPoints;
    const double step = static_cast<double>(result.pointCount) / maxPoints;
    result.keys1.reserve(maxPoints);
    result.values1.reserve(maxPoints);
    result.keys2.reserve(maxPoints);
    result.values2.reserve(maxPoints);
    result.keys3.reserve(maxPoints);
    result.values3.reserve(maxPoints);

    int rowIndex = 0;
    double nextSample = 0.0;
    while (query.next()) {
        if (rowIndex >= static_cast<int>(nextSample)) {
            appendRow(query, result);
            nextSample += step;
        }
        ++rowIndex;
    }
    result.sampledCount = result.keys1.size();
    return result;
}

HistoryTrendCountResult HistoryDataReader::countTrendData(const QString& dbPath,
                                                           int deviceId,
                                                           qint64 startMs,
                                                           qint64 endMs)
{
    HistoryTrendCountResult result;
    QSqlDatabase db = databaseForCurrentThread(
        dbPath, QStringLiteral("trend_cnt"), false, result.error);
    if (!db.isOpen() && !result.error.isEmpty()) {
        result.failure = HistoryReadFailure::Open;
        return result;
    }

    QSqlQuery query(db);
    const QString sql = QStringLiteral(
        "SELECT COUNT(*) FROM history_data WHERE device_id = :device_id "
        "AND timestamp >= :start AND timestamp <= :end");
    if (!query.prepare(sql)) {
        result.failure = HistoryReadFailure::Prepare;
        result.error = query.lastError().text();
        return result;
    }
    query.bindValue(":device_id", deviceId);
    query.bindValue(":start", startMs);
    query.bindValue(":end", endMs);
    if (!query.exec() || !query.next()) {
        result.failure = HistoryReadFailure::Execute;
        result.error = query.lastError().text();
        return result;
    }

    result.pointCount = query.value(0).toInt();
    return result;
}

HistoryTrendBatchResult HistoryDataReader::readTrendBatch(
    const QString& dbPath, int deviceId, qint64 startMs, qint64 endMs,
    const QString& deviceType, qint64 offset, int batchSize)
{
    HistoryTrendBatchResult result;
    QSqlDatabase db = databaseForCurrentThread(
        dbPath, QStringLiteral("trend_batch"), false, result.error);
    if (!db.isOpen() && !result.error.isEmpty()) {
        result.failure = HistoryReadFailure::Open;
        return result;
    }

    QString valueColumns;
    if (deviceType == "TANK") {
        valueColumns = QStringLiteral("pressure, temperature, 0");
    } else if (deviceType == "DRY") {
        valueColumns = QStringLiteral("pressure, outlet_pressure, dew_point");
    } else {
        valueColumns = QStringLiteral("temperature, pressure, phase_current");
    }

    const QString sql = QStringLiteral(
        "SELECT history_time, %1 FROM history_data "
        "WHERE device_id = :device_id AND timestamp >= :start AND timestamp <= :end "
        "ORDER BY timestamp ASC LIMIT :limit OFFSET :offset").arg(valueColumns);
    QSqlQuery query(db);
    if (!query.prepare(sql)) {
        result.failure = HistoryReadFailure::Prepare;
        result.error = query.lastError().text();
        return result;
    }
    query.bindValue(":device_id", deviceId);
    query.bindValue(":start", startMs);
    query.bindValue(":end", endMs);
    query.bindValue(":limit", batchSize);
    query.bindValue(":offset", offset);
    if (!query.exec()) {
        result.failure = HistoryReadFailure::Execute;
        result.error = query.lastError().text();
        return result;
    }

    while (query.next()) {
        const QString timeString = query.value(0).toString();
        QDateTime timestamp = QDateTime::fromString(timeString, "yyyy-MM-dd hh:mm:ss");
        if (!timestamp.isValid()) {
            timestamp = QDateTime::fromString(timeString, "yyyy-MM-dd HH:mm:ss");
        }
        if (!timestamp.isValid()) {
            timestamp = QDateTime::fromString(timeString, Qt::ISODate);
        }
        if (!timestamp.isValid()) {
            continue;
        }

        result.keys.append(timestamp.toMSecsSinceEpoch() / 1000.0);
        result.values1.append(query.value(1).toDouble());
        result.values2.append(query.value(2).toDouble());
        result.values3.append(query.value(3).toDouble());
    }

    result.isLastBatch = result.keys.size() < batchSize;
    return result;
}

LatestHistoryValues HistoryDataReader::readLatestValues(const QString& dbPath,
                                                         int deviceId)
{
    LatestHistoryValues result;
    QSqlDatabase db = databaseForCurrentThread(
        dbPath, QStringLiteral("trend_rt"), false, result.error);
    if (!db.isOpen() && !result.error.isEmpty()) {
        result.failure = HistoryReadFailure::Open;
        return result;
    }

    const QString sql = QStringLiteral(
        "SELECT history_time, temperature, pressure, phase_current, outlet_pressure, dew_point "
        "FROM history_data WHERE device_id = :device_id ORDER BY timestamp DESC LIMIT 1");
    QSqlQuery query(db);
    if (!query.prepare(sql)) {
        result.failure = HistoryReadFailure::Prepare;
        result.error = query.lastError().text();
        return result;
    }
    query.bindValue(":device_id", deviceId);
    if (!query.exec()) {
        result.failure = HistoryReadFailure::Execute;
        result.error = query.lastError().text();
        return result;
    }
    if (!query.next()) {
        return result;
    }

    result.found = true;
    result.temperature = query.value(1).toDouble();
    result.pressure = query.value(2).toDouble();
    result.phaseCurrent = query.value(3).toDouble();
    result.outletPressure = query.value(4).toDouble();
    result.dewPoint = query.value(5).toDouble();
    return result;
}

void HistoryDataReader::closeCurrentThreadConnections()
{
    const QStringList prefixes{QStringLiteral("history_worker"),
                               QStringLiteral("trend_cnt"),
                               QStringLiteral("trend_batch"),
                               QStringLiteral("trend_rt")};
    for (const QString& prefix : prefixes) {
        const QString connectionName = connectionNameForCurrentThread(prefix);
        if (!QSqlDatabase::contains(connectionName)) {
            continue;
        }

        QSqlDatabase db = QSqlDatabase::database(connectionName);
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
    }
}

}
