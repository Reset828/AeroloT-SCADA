#ifndef DATASTORE_DATABASEMANAGER_H
#define DATASTORE_DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariant>
#include <QMutex>
#include <QVector>
#include "records.h"
#include "Application/alarm_record_writer.h"

namespace DataStore {

class DatabaseManager : public QObject, public Application::AlarmRecordWriter {
    Q_OBJECT
public:
    static DatabaseManager& instance();

    bool initialize(const QString& dbPath);
    void initializeWorkerConnection(const QString& dbPath);
    void close();

    QSqlDatabase getDatabase() { return m_database; }

    bool execute(const QString& sql);
    QSqlQuery query(const QString& sql);

    // 清理过期数据：删除days天前的历史数据和报警日志
    // historyData保留30天，alarmLog保留90天
    bool cleanupOldData(int historyDays = 30, int alarmDays = 90);

    // 通信日志相关操作
    bool insertCommLog(qint64 timestamp, const QString& logType, const QString& direction, const QString& deviceId, const QString& content);
    AlarmInsertStatus insertAlarmLog(const AlarmLogEntry& record);
    Application::AlarmRecordWriteStatus writeAlarmRecord(
        const core::AlarmRecord& record) override;
    QVector<CommLogEntry> getCommLogs(qint64 startTimestamp = 0,
                                      qint64 endTimestamp = 0,
                                      int limit = 1000);

    QString databasePath() const { return m_dbPath; }

signals:
    void initializationFailed(const QString& error);

private:
    DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool createTables();
    bool createIndexes();

    QString m_dbPath;
    QSqlDatabase m_database;
    QMutex m_mutex;
};

}
#endif
