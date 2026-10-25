#include "alarm_log_reader.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

namespace DataStore {
namespace {

QString connectionNameForCurrentThread()
{
    return QStringLiteral("alarm_query_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

}

AlarmLogReadResult AlarmLogReader::read(const QString& dbPath, qint64 startMs,
                                       qint64 endMs)
{
    AlarmLogReadResult result;
    const QString connectionName = connectionNameForCurrentThread();
    QSqlDatabase db;
    if (QSqlDatabase::contains(connectionName)) {
        db = QSqlDatabase::database(connectionName);
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            result.failure = AlarmLogReadFailure::Open;
            result.error = db.lastError().text();
            return result;
        }
        db.exec("PRAGMA journal_mode=WAL");
        db.exec("PRAGMA read_uncommitted=1");
    }

    const QString sql = QStringLiteral(
        "SELECT device_id, alarm_time, alarm_type, message, alarm_level FROM alarm_log "
        "WHERE timestamp >= :start AND timestamp <= :end "
        "ORDER BY timestamp DESC");
    QSqlQuery query(db);
    if (!query.prepare(sql)) {
        result.failure = AlarmLogReadFailure::Prepare;
        result.error = query.lastError().text();
        return result;
    }
    query.bindValue(":start", startMs);
    query.bindValue(":end", endMs);
    if (!query.exec()) {
        result.failure = AlarmLogReadFailure::Execute;
        result.error = query.lastError().text();
        return result;
    }

    while (query.next()) {
        AlarmLogEntry record;
        record.deviceId = query.value(0).toInt();
        record.alarmTime = query.value(1).toString();
        record.alarmType = query.value(2).toInt();
        record.message = query.value(3).toString();
        record.alarmLevel = query.value(4).toInt();
        result.records.append(record);
    }
    return result;
}

void AlarmLogReader::closeCurrentThreadConnection()
{
    const QString connectionName = connectionNameForCurrentThread();
    if (!QSqlDatabase::contains(connectionName)) {
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(connectionName);
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

}
