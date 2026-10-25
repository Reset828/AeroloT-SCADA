#include "database_manager.h"

#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <QDateTime>
#include <QThread>

namespace DataStore {

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::initialize(const QString& dbPath) {
    QMutexLocker locker(&m_mutex);

    m_dbPath = dbPath;
    m_database = QSqlDatabase::addDatabase("QSQLITE");
    m_database.setDatabaseName(dbPath);

    if (!m_database.open()) {
        QString error = m_database.lastError().text();
        emit initializationFailed(error);
        return false;
    }

    QSqlQuery query(m_database);

    query.exec("PRAGMA journal_mode=WAL");
    query.exec("PRAGMA synchronous=NORMAL");
    query.exec("PRAGMA cache_size=10000");
    query.exec("PRAGMA busy_timeout=5000");

    if (!createTables()) {
        QString error = "Failed to create tables";
        emit initializationFailed(error);
        return false;
    }

    if (!createIndexes()) {
        qWarning() << "Failed to create some indexes";
    }

    qDebug() << "Database initialized successfully at:" << dbPath;
    return true;
}

void DatabaseManager::initializeWorkerConnection(const QString& dbPath) {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "worker_connection");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "Failed to open worker database connection:" << db.lastError().text();
        return;
    }
    
    QSqlQuery query(db);
    query.exec("PRAGMA journal_mode=WAL");
    query.exec("PRAGMA synchronous=NORMAL");
    query.exec("PRAGMA cache_size=10000");
    query.exec("PRAGMA busy_timeout=5000");
}

void DatabaseManager::close() {
    QMutexLocker locker(&m_mutex);
    if (m_database.isOpen()) {
        m_database.close();
    }
}

bool DatabaseManager::execute(const QString& sql) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_database);
    return query.exec(sql);
}

QSqlQuery DatabaseManager::query(const QString& sql) {
    QMutexLocker locker(&m_mutex);
    return QSqlQuery(sql, m_database);
}

bool DatabaseManager::cleanupOldData(int historyDays, int alarmDays) {
    // 计算时间戳阈值
    int64_t now = QDateTime::currentSecsSinceEpoch();
    int64_t historyThreshold = now - (static_cast<int64_t>(historyDays) * 24 * 60 * 60);
    int64_t alarmThreshold = now - (static_cast<int64_t>(alarmDays) * 24 * 60 * 60);

    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_database);

    // 每批删除的记录数，避免长时间锁库
    const int batchSize = 1000;

    // 分批删除历史数据
    int totalDeleted = 0;
    while (true) {
        query.prepare(QString("DELETE FROM history_data WHERE timestamp < ? LIMIT %1").arg(batchSize));
        query.bindValue(0, historyThreshold);
        if (!query.exec()) {
            qWarning() << "Failed to cleanup history data:" << query.lastError().text();
            break;
        }
        int deleted = query.numRowsAffected();
        totalDeleted += deleted;
        if (deleted < batchSize) {
            break;
        }
    }

    // 分批删除报警日志
    while (true) {
        query.prepare(QString("DELETE FROM alarm_log WHERE timestamp < ? LIMIT %1").arg(batchSize));
        query.bindValue(0, alarmThreshold);
        if (!query.exec()) {
            qWarning() << "Failed to cleanup alarm data:" << query.lastError().text();
            break;
        }
        int deleted = query.numRowsAffected();
        totalDeleted += deleted;
        if (deleted < batchSize) {
            break;
        }
    }

    qDebug() << "Cleanup completed, total deleted:" << totalDeleted;
    return true;
}

bool DatabaseManager::createTables() {
    QSqlQuery query(m_database);

    QString createHistoryTable = R"(
        CREATE TABLE IF NOT EXISTS history_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            history_time TEXT NOT NULL,
            device_id INTEGER NOT NULL,
            device_type INTEGER NOT NULL,
            pressure REAL,
            temperature REAL,
            status INTEGER,
            runtime_hours REAL,
            vfd_frequency REAL,
            phase_current REAL,
            alarm_code INTEGER,
            outlet_pressure REAL,
            dew_point REAL,
            regen_status INTEGER,
            created_at INTEGER DEFAULT (strftime('%s', 'now'))
        )
    )";

    QString createAlarmTable = R"(
        CREATE TABLE IF NOT EXISTS alarm_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            device_id INTEGER NOT NULL,
            device_type INTEGER NOT NULL,
            alarm_type INTEGER NOT NULL,
            alarm_level INTEGER NOT NULL,
            value REAL,
            threshold REAL,
            message TEXT,
            alarm_time TEXT,
            ack_time INTEGER,
            ack_user TEXT,
            created_at INTEGER DEFAULT (strftime('%s', 'now'))
        )
    )";

    QString createAuditTable = R"(
        CREATE TABLE IF NOT EXISTS audit_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            user_name TEXT NOT NULL,
            operation_type INTEGER NOT NULL,
            target_device_id INTEGER,
            target_parameter TEXT,
            old_value TEXT,
            new_value TEXT,
            result INTEGER,
            error_message TEXT,
            created_at INTEGER DEFAULT (strftime('%s', 'now'))
        )
    )";

    QString createCommLogTable = R"(
        CREATE TABLE IF NOT EXISTS comm_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            log_type TEXT NOT NULL,
            direction TEXT NOT NULL,
            device_id TEXT NOT NULL,
            content TEXT,
            created_at INTEGER DEFAULT (strftime('%s', 'now'))
        )
    )";

    if (!query.exec(createHistoryTable)) {
        qWarning() << "Failed to create history_data table:" << query.lastError().text();
        return false;
    }

    if (!query.exec(createAlarmTable)) {
        qWarning() << "Failed to create alarm_log table:" << query.lastError().text();
        return false;
    }

    if (!query.exec(createAuditTable)) {
        qWarning() << "Failed to create audit_log table:" << query.lastError().text();
        return false;
    }

    if (!query.exec(createCommLogTable)) {
        qWarning() << "Failed to create comm_log table:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::createIndexes() {
    QSqlQuery query(m_database);

    query.exec("CREATE INDEX IF NOT EXISTS idx_history_timestamp ON history_data(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_history_device ON history_data(device_id, timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_alarm_timestamp ON alarm_log(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_alarm_device ON alarm_log(device_id, timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_audit_timestamp ON audit_log(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_comm_timestamp ON comm_log(timestamp)");

    return true;
}

bool DatabaseManager::insertCommLog(qint64 timestamp, const QString& logType, const QString& direction, const QString& deviceId, const QString& content) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_database);
    query.prepare("INSERT INTO comm_log (timestamp, log_type, direction, device_id, content) VALUES (?, ?, ?, ?, ?)");
    query.bindValue(0, timestamp);
    query.bindValue(1, logType);
    query.bindValue(2, direction);
    query.bindValue(3, deviceId);
    query.bindValue(4, content);
    return query.exec();
}

AlarmInsertStatus DatabaseManager::insertAlarmLog(const AlarmLogEntry& record) {
    const QString connectionName = QStringLiteral("alarm_insert_%1")
        .arg(quintptr(QThread::currentThreadId()));
    QSqlDatabase db;
    if (QSqlDatabase::contains(connectionName)) {
        db = QSqlDatabase::database(connectionName);
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(databasePath());
        if (!db.open()) {
            qWarning() << "Failed to open database for alarm insert:" << db.lastError().text();
            return AlarmInsertStatus::ConnectionFailed;
        }
        db.exec("PRAGMA journal_mode=WAL");
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO alarm_log (timestamp, device_id, device_type, alarm_type, alarm_level, value, threshold, message, alarm_time) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.bindValue(0, record.timestamp);
    query.bindValue(1, record.deviceId);
    query.bindValue(2, record.deviceType);
    query.bindValue(3, record.alarmType);
    query.bindValue(4, record.alarmLevel);
    query.bindValue(5, record.value);
    query.bindValue(6, record.threshold);
    query.bindValue(7, record.message);
    query.bindValue(8, record.alarmTime);

    if (!query.exec()) {
        qWarning() << "Failed to insert alarm record:" << query.lastError().text();
        return AlarmInsertStatus::InsertFailed;
    }
    return AlarmInsertStatus::Inserted;
}

Application::AlarmRecordWriteStatus DatabaseManager::writeAlarmRecord(
    const core::AlarmRecord& record) {
    const AlarmLogEntry logEntry{
        record.timestamp,
        record.deviceId,
        record.deviceType,
        record.alarmType,
        record.alarmLevel,
        record.value,
        record.threshold,
        record.message,
        record.alarmTime
    };

    switch (insertAlarmLog(logEntry)) {
    case AlarmInsertStatus::ConnectionFailed:
        return Application::AlarmRecordWriteStatus::ConnectionFailed;
    case AlarmInsertStatus::InsertFailed:
        return Application::AlarmRecordWriteStatus::InsertFailed;
    case AlarmInsertStatus::Inserted:
        return Application::AlarmRecordWriteStatus::Inserted;
    }

    return Application::AlarmRecordWriteStatus::InsertFailed;
}

QVector<CommLogEntry> DatabaseManager::getCommLogs(qint64 startTimestamp,
                                                   qint64 endTimestamp,
                                                   int limit) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_database);
    QString sql = "SELECT timestamp, log_type, direction, device_id, content FROM comm_log WHERE 1=1";
    if (startTimestamp > 0) {
        sql += " AND timestamp >= ?";
    }
    if (endTimestamp > 0) {
        sql += " AND timestamp <= ?";
    }
    sql += " ORDER BY timestamp DESC LIMIT ?";
    query.prepare(sql);
    int bindIndex = 0;
    if (startTimestamp > 0) query.bindValue(bindIndex++, startTimestamp);
    if (endTimestamp > 0) query.bindValue(bindIndex++, endTimestamp);
    query.bindValue(bindIndex, limit);
    query.exec();

    QVector<CommLogEntry> records;
    while (query.next()) {
        CommLogEntry record;
        record.timestamp = query.value(0).toLongLong();
        record.logType = query.value(1).toString();
        record.direction = query.value(2).toString();
        record.deviceId = query.value(3).toString();
        record.content = query.value(4).toString();
        records.append(record);
    }
    return records;
}

}
