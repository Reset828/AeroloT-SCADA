#ifndef DATASTORE_DATASTORE_H
#define DATASTORE_DATASTORE_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QMap>
#include <QString>
#include <QDateTime>
#include <QMetaType>
#include <atomic>
#include <vector>
#include "records.h"
#include "Application/device_data_ports.h"
#include "../core/device_data.h"

namespace DataStore {

using DeviceData = core::DeviceData;

class DataWriteWorker : public QObject {
    Q_OBJECT
public:
    explicit DataWriteWorker(QObject* parent = nullptr);
    ~DataWriteWorker();

public slots:
    void initializeDatabase(const QString& dbPath);
    void appendHistory(const HistoryRecord& record);
    void appendAlarm(const AlarmRecord& record);
    void appendAudit(const AuditRecord& record);
    bool insertCommLog(qint64 timestamp, const QString& logType, const QString& direction, const QString& deviceId, const QString& content);
    void updateCurrentData(const QString& deviceId, const DeviceData& data);
    void flush();
    void forceFlush();

signals:
    void historyWritten(int count);
    void alarmWritten(int count);
    void auditWritten(int count);
    void writeError(const QString& error);
    void currentDataUpdated(const QMap<QString, DeviceData>& data);
    void databaseInitialized(bool success);

private slots:
    void onFlushTimerTimeout();

private:
    bool flushHistory();
    bool flushAlarms();
    bool flushAudits();
    bool ensureDatabaseOpen();

    static constexpr int HISTORY_BUFFER_CAPACITY = 100;
    static constexpr int ALARM_BUFFER_CAPACITY = 50;
    static constexpr int AUDIT_BUFFER_CAPACITY = 50;
    static constexpr int FLUSH_INTERVAL_MS = 5000;
    static constexpr const char* WORKER_CONNECTION_NAME = "worker_connection";

    std::vector<HistoryRecord> m_historyBuffer;
    std::vector<AlarmRecord> m_alarmBuffer;
    std::vector<AuditRecord> m_auditBuffer;
    QMap<QString, DeviceData> m_currentData;

    mutable QMutex m_mutex;
    QTimer* m_flushTimer;
    QString m_dbPath;
    std::atomic<bool> m_dbInitialized{false};
};

class DataStore : public QObject,
                  public Application::CurrentDeviceDataWriter {
    Q_OBJECT
public:
    static DataStore& instance();

    bool initialize(const QString& dbPath);
    void shutdown();
    QString databasePath() const;
    bool cleanupOldData(int historyDays = 30, int alarmDays = 90);

    QMap<QString, DeviceData> currentData() const;
    void updateCurrentData(const QString& deviceId,
                           const DeviceData& data) override;
    void flush();

    void appendHistory(const HistoryRecord& record);
    int appendCurrentHistorySnapshot();
    void appendAlarm(const AlarmRecord& record);
    void appendAudit(const AuditRecord& record);
    
    bool insertCommLog(qint64 timestamp, const QString& logType, const QString& direction, const QString& deviceId, const QString& content);
    QVector<CommLogEntry> getCommLogs(qint64 startTimestamp, qint64 endTimestamp,
                                      int limit = 1000);

signals:
    void historyWritten(int count);
    void alarmWritten(int count);
    void auditWritten(int count);
    void currentDataUpdated(const QMap<QString, DeviceData>& data);
    void writeError(const QString& error);

private:
    explicit DataStore(QObject* parent = nullptr);
    ~DataStore();
    DataStore(const DataStore&) = delete;
    DataStore& operator=(const DataStore&) = delete;

    QThread* m_workerThread;
    DataWriteWorker* m_worker;
    mutable QMutex m_dataMutex;
    QMap<QString, DeviceData> m_cachedCurrentData;
};

}

#endif
