#include "datastore.h"
#include "database_manager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QSqlDatabase>
#include <QUuid>
#include <QMetaType>
#include <QDateTime>
#include <QList>
#include <algorithm>

namespace DataStore {

namespace {
    static const bool s_metaTypesRegistered = []() {
        qRegisterMetaType<HistoryRecord>("HistoryRecord");
        qRegisterMetaType<AlarmRecord>("AlarmRecord");
        qRegisterMetaType<AuditRecord>("AuditRecord");
        qRegisterMetaType<DeviceData>("DeviceData");
        qRegisterMetaType<QMap<QString, DeviceData>>("QMap<QString,DeviceData>");
        return true;
    }();
}

DataWriteWorker::DataWriteWorker(QObject* parent)
    : QObject(parent)
    , m_flushTimer(new QTimer(this))
{
    m_historyBuffer.reserve(HISTORY_BUFFER_CAPACITY * 2);
    m_alarmBuffer.reserve(ALARM_BUFFER_CAPACITY * 2);
    m_auditBuffer.reserve(AUDIT_BUFFER_CAPACITY * 2);

    m_flushTimer->setInterval(FLUSH_INTERVAL_MS);
    connect(m_flushTimer, &QTimer::timeout, this, &DataWriteWorker::onFlushTimerTimeout);
    m_flushTimer->start();
    
    qDebug() << "DataWriteWorker created in thread:" << QThread::currentThreadId();
}

DataWriteWorker::~DataWriteWorker() {
    qDebug() << "DataWriteWorker destructor - forcing final flush";
    forceFlush();
    
    if (QSqlDatabase::contains(WORKER_CONNECTION_NAME)) {
        QSqlDatabase::database(WORKER_CONNECTION_NAME).close();
        QSqlDatabase::removeDatabase(WORKER_CONNECTION_NAME);
    }
}

void DataWriteWorker::initializeDatabase(const QString& dbPath) {
    qDebug() << "initializeDatabase called in thread:" << QThread::currentThreadId() 
             << "dbPath:" << dbPath;
    
    m_dbPath = dbPath;
    
    if (QSqlDatabase::contains(WORKER_CONNECTION_NAME)) {
        QSqlDatabase::database(WORKER_CONNECTION_NAME).close();
        QSqlDatabase::removeDatabase(WORKER_CONNECTION_NAME);
    }
    
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", WORKER_CONNECTION_NAME);
    db.setDatabaseName(dbPath);
    
    if (!db.open()) {
        QString error = QString("Failed to open database: %1").arg(db.lastError().text());
        qCritical() << error;
        emit writeError(error);
        emit databaseInitialized(false);
        return;
    }
    
    QSqlQuery query(db);
    
    if (!query.exec("PRAGMA journal_mode=WAL")) {
        qWarning() << "Failed to set WAL mode:" << query.lastError().text();
    } else {
        qDebug() << "WAL mode enabled for worker connection";
    }
    
    query.exec("PRAGMA synchronous=NORMAL");
    query.exec("PRAGMA cache_size=10000");
    query.exec("PRAGMA busy_timeout=5000");
    query.exec("PRAGMA temp_store=MEMORY");
    
    m_dbInitialized = true;
    
    qDebug() << "Worker database connection initialized successfully";
    emit databaseInitialized(true);
}

bool DataWriteWorker::ensureDatabaseOpen() {
    if (!m_dbInitialized) {
        emit writeError("Database not initialized");
        return false;
    }
    
    QSqlDatabase db = QSqlDatabase::database(WORKER_CONNECTION_NAME);
    if (!db.isOpen()) {
        if (!db.open()) {
            emit writeError(QString("Failed to reopen database: %1").arg(db.lastError().text()));
            return false;
        }
    }
    
    return true;
}

void DataWriteWorker::appendHistory(const HistoryRecord& record) {
    QMutexLocker locker(&m_mutex);
    m_historyBuffer.push_back(record);
    
    if (m_historyBuffer.size() >= HISTORY_BUFFER_CAPACITY) {
        qDebug() << "History buffer full (" << m_historyBuffer.size() << "), triggering flush";
        locker.unlock();
        flushHistory();
    }
}

void DataWriteWorker::appendAlarm(const AlarmRecord& record) {
    QMutexLocker locker(&m_mutex);
    m_alarmBuffer.push_back(record);
    
    if (m_alarmBuffer.size() >= ALARM_BUFFER_CAPACITY) {
        qDebug() << "Alarm buffer full (" << m_alarmBuffer.size() << "), triggering flush";
        locker.unlock();
        flushAlarms();
    }
}

void DataWriteWorker::appendAudit(const AuditRecord& record) {
    QMutexLocker locker(&m_mutex);
    m_auditBuffer.push_back(record);
    
    if (m_auditBuffer.size() >= AUDIT_BUFFER_CAPACITY) {
        qDebug() << "Audit buffer full (" << m_auditBuffer.size() << "), triggering flush";
        locker.unlock();
        flushAudits();
    }
}

void DataWriteWorker::flush() {
    QMutexLocker locker(&m_mutex);
    flushHistory();
    flushAlarms();
    flushAudits();
}

void DataWriteWorker::forceFlush() {
    qDebug() << "forceFlush called";
    QMutexLocker locker(&m_mutex);
    
    if (!ensureDatabaseOpen()) {
        return;
    }
    
    flushHistory();
    flushAlarms();
    flushAudits();
    
    QSqlDatabase db = QSqlDatabase::database(WORKER_CONNECTION_NAME);
    QSqlQuery query(db);
    query.exec("PRAGMA wal_checkpoint(TRUNCATE)");
    
    qDebug() << "forceFlush completed";
}

void DataWriteWorker::updateCurrentData(const QString& deviceId, const DeviceData& data) {
    QMutexLocker locker(&m_mutex);
    m_currentData[deviceId] = data;
    emit currentDataUpdated(m_currentData);
}

bool DataWriteWorker::insertCommLog(qint64 timestamp, const QString& logType, const QString& direction, const QString& deviceId, const QString& content) {
    return DatabaseManager::instance().insertCommLog(timestamp, logType, direction, deviceId, content);
}

void DataWriteWorker::onFlushTimerTimeout() {
    qDebug() << "Flush timer triggered - checking buffers:"
             << "history:" << m_historyBuffer.size()
             << "alarm:" << m_alarmBuffer.size()
             << "audit:" << m_auditBuffer.size();
    
    QMutexLocker locker(&m_mutex);
    
    if (!ensureDatabaseOpen()) {
        return;
    }
    
    flushHistory();
    flushAlarms();
    flushAudits();
}

bool DataWriteWorker::flushHistory() {
    if (m_historyBuffer.empty()) {
        return true;
    }
    
    QSqlDatabase db = QSqlDatabase::database(WORKER_CONNECTION_NAME);
    if (!db.isOpen()) {
        emit writeError("Database not open for history flush");
        return false;
    }
    
    if (!db.transaction()) {
        emit writeError(QString("Failed to start transaction for history: %1").arg(db.lastError().text()));
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO history_data 
        (timestamp, history_time, device_id, device_type, pressure, temperature, status, 
         runtime_hours, vfd_frequency, phase_current, alarm_code, outlet_pressure, dew_point, regen_status)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    int successCount = 0;
    for (const auto& record : m_historyBuffer) {
        query.bindValue(0, QVariant::fromValue(record.timestamp));
        query.bindValue(1, record.historyTime);
        query.bindValue(2, record.deviceId);
        query.bindValue(3, static_cast<int>(record.deviceType));
        query.bindValue(4, record.pressure);
        query.bindValue(5, record.temperature);
        query.bindValue(6, record.status);
        query.bindValue(7, record.runtimeHours);
        query.bindValue(8, record.vfdFrequency);
        query.bindValue(9, record.phaseCurrent);
        query.bindValue(10, record.alarmCode);
        query.bindValue(11, record.outletPressure);
        query.bindValue(12, record.dewPoint);
        query.bindValue(13, record.regenStatus);
        
        if (query.exec()) {
            successCount++;
        } else {
            qWarning() << "Failed to insert history record:" << query.lastError().text();
        }
    }

    if (!db.commit()) {
        emit writeError(QString("Failed to commit history data: %1").arg(db.lastError().text()));
        db.rollback();
        return false;
    }

    int count = m_historyBuffer.size();
    m_historyBuffer.clear();
    m_historyBuffer.reserve(HISTORY_BUFFER_CAPACITY * 2);
    
    qDebug() << "flushHistory: Successfully wrote" << successCount << "/" << count << "records";
    emit historyWritten(count);
    return true;
}

bool DataWriteWorker::flushAlarms() {
    if (m_alarmBuffer.empty()) {
        return true;
    }

    QSqlDatabase db = QSqlDatabase::database(WORKER_CONNECTION_NAME);
    if (!db.isOpen()) {
        emit writeError("Database not open for alarm flush");
        return false;
    }
    
    if (!db.transaction()) {
        emit writeError(QString("Failed to start transaction for alarms: %1").arg(db.lastError().text()));
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO alarm_log 
        (timestamp, device_id, device_type, alarm_type, alarm_level, 
         value, threshold, message, ack_time, ack_user)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    int successCount = 0;
    for (const auto& record : m_alarmBuffer) {
        query.bindValue(0, QVariant::fromValue(record.timestamp));
        query.bindValue(1, record.deviceId);
        query.bindValue(2, static_cast<int>(record.deviceType));
        query.bindValue(3, record.alarmType);
        query.bindValue(4, static_cast<int>(record.alarmLevel));
        query.bindValue(5, record.value);
        query.bindValue(6, record.threshold);
        query.bindValue(7, record.message);
        query.bindValue(8, QVariant::fromValue(record.ackTime));
        query.bindValue(9, record.ackUser);
        
        if (query.exec()) {
            successCount++;
        } else {
            qWarning() << "Failed to insert alarm record:" << query.lastError().text();
        }
    }

    if (!db.commit()) {
        emit writeError(QString("Failed to commit alarm data: %1").arg(db.lastError().text()));
        db.rollback();
        return false;
    }

    int count = m_alarmBuffer.size();
    m_alarmBuffer.clear();
    m_alarmBuffer.reserve(ALARM_BUFFER_CAPACITY * 2);
    
    qDebug() << "flushAlarms: Successfully wrote" << successCount << "/" << count << "records";
    emit alarmWritten(count);
    return true;
}

bool DataWriteWorker::flushAudits() {
    if (m_auditBuffer.empty()) {
        return true;
    }

    QSqlDatabase db = QSqlDatabase::database(WORKER_CONNECTION_NAME);
    if (!db.isOpen()) {
        emit writeError("Database not open for audit flush");
        return false;
    }
    
    if (!db.transaction()) {
        emit writeError(QString("Failed to start transaction for audit: %1").arg(db.lastError().text()));
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO audit_log 
        (timestamp, user_name, operation_type, target_device_id, 
         target_parameter, old_value, new_value, result, error_message)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    int successCount = 0;
    for (const auto& record : m_auditBuffer) {
        query.bindValue(0, QVariant::fromValue(record.timestamp));
        query.bindValue(1, record.userName);
        query.bindValue(2, record.operationType);
        query.bindValue(3, record.targetDeviceId);
        query.bindValue(4, record.targetParameter);
        query.bindValue(5, record.oldValue);
        query.bindValue(6, record.newValue);
        query.bindValue(7, record.result);
        query.bindValue(8, record.errorMessage);
        
        if (query.exec()) {
            successCount++;
        } else {
            qWarning() << "Failed to insert audit record:" << query.lastError().text();
        }
    }

    if (!db.commit()) {
        emit writeError(QString("Failed to commit audit data: %1").arg(db.lastError().text()));
        db.rollback();
        return false;
    }

    int count = m_auditBuffer.size();
    m_auditBuffer.clear();
    m_auditBuffer.reserve(AUDIT_BUFFER_CAPACITY * 2);
    
    qDebug() << "flushAudits: Successfully wrote" << successCount << "/" << count << "records";
    emit auditWritten(count);
    return true;
}

DataStore& DataStore::instance() {
    static DataStore instance;
    return instance;
}

DataStore::DataStore(QObject* parent)
    : QObject(parent)
    , m_workerThread(new QThread(this))
    , m_worker(new DataWriteWorker())
    , m_cachedCurrentData()
{
    m_worker->moveToThread(m_workerThread);
    
    connect(m_worker, &DataWriteWorker::historyWritten, this, &DataStore::historyWritten);
    connect(m_worker, &DataWriteWorker::alarmWritten, this, &DataStore::alarmWritten);
    connect(m_worker, &DataWriteWorker::auditWritten, this, &DataStore::auditWritten);
    connect(m_worker, &DataWriteWorker::currentDataUpdated, this, &DataStore::currentDataUpdated);
    connect(m_worker, &DataWriteWorker::currentDataUpdated, this, [&](const QMap<QString, DeviceData>& data) {
        QMutexLocker locker(&m_dataMutex);
        m_cachedCurrentData = data;
    });
    connect(m_worker, &DataWriteWorker::writeError, this, &DataStore::writeError);
    connect(m_worker, &DataWriteWorker::databaseInitialized, this, [](bool success) {
        qDebug() << "Database initialization in worker thread:" << (success ? "SUCCESS" : "FAILED");
    });
    
    m_workerThread->start();
    
    QThread::msleep(100);
    
    qDebug() << "DataStore initialized, worker thread running";
}

DataStore::~DataStore() {
    shutdown();
}

bool DataStore::initialize(const QString& dbPath) {
    qDebug() << "DataStore::initialize called with dbPath:" << dbPath;
    
    bool mainDbOk = DatabaseManager::instance().initialize(dbPath);
    if (!mainDbOk) {
        qCritical() << "Failed to initialize main database connection";
        return false;
    }
    
    QEventLoop loop;
    bool initSuccess = false;
    
    QMetaObject::Connection conn = connect(m_worker, &DataWriteWorker::databaseInitialized, 
        &loop, [&](bool success) {
            initSuccess = success;
            loop.quit();
        });
    
    QMetaObject::invokeMethod(m_worker, "initializeDatabase", Qt::QueuedConnection,
                              Q_ARG(QString, dbPath));
    
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(5000);
    
    loop.exec();
    
    disconnect(conn);
    
    if (!initSuccess) {
        qCritical() << "Failed to initialize worker database connection";
        return false;
    }
    
    qDebug() << "DataStore initialization completed successfully";
    return true;
}

QString DataStore::databasePath() const {
    return DatabaseManager::instance().databasePath();
}

bool DataStore::cleanupOldData(int historyDays, int alarmDays) {
    return DatabaseManager::instance().cleanupOldData(historyDays, alarmDays);
}

void DataStore::shutdown() {
    qDebug() << "DataStore::shutdown called";
    
    if (m_workerThread->isRunning()) {
        QMetaObject::invokeMethod(m_worker, "forceFlush", Qt::QueuedConnection);
        
        QThread::msleep(200);
        
        QEventLoop loop;
        connect(m_workerThread, &QThread::finished, &loop, &QEventLoop::quit);
        m_workerThread->quit();
        
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(3000);
        
        loop.exec();
        
        if (m_workerThread->isRunning()) {
            qWarning() << "Worker thread did not finish gracefully, terminating";
            m_workerThread->terminate();
            m_workerThread->wait();
        }
    }
    
    qDebug() << "DataStore shutdown completed";
}

void DataStore::flush() {
    QMetaObject::invokeMethod(m_worker, "flush", Qt::BlockingQueuedConnection);
}

void DataStore::appendHistory(const HistoryRecord& record) {
    QMetaObject::invokeMethod(m_worker, "appendHistory", Qt::QueuedConnection,
                              Q_ARG(HistoryRecord, record));
}

int DataStore::appendCurrentHistorySnapshot() {
    const auto allData = currentData();
    qDebug() << "saveAllDeviceHistory: 当前数据中的设备数=" << allData.size();

    QList<DeviceData> devices;
    for (auto it = allData.constBegin(); it != allData.constEnd(); ++it) {
        devices.append(it.value());
        qDebug() << "  - 设备" << it.key() << "deviceId=" << it.value().deviceId
                 << "pressure=" << it.value().pressure;
    }

    std::sort(devices.begin(), devices.end(), [](const DeviceData& left,
                                                 const DeviceData& right) {
        return left.deviceId < right.deviceId;
    });

    const qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    const QString historyTime =
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    for (const DeviceData& data : devices) {
        HistoryRecord record;
        record.timestamp = timestamp;
        record.historyTime = historyTime;
        record.deviceId = data.deviceId;
        record.deviceType = data.deviceType;
        record.pressure = data.pressure;
        record.temperature = data.temperature;
        record.status = data.status;
        record.runtimeHours = data.runtimeHours;
        record.vfdFrequency = data.vfdFrequency;
        record.phaseCurrent = data.phaseCurrent;
        record.alarmCode = data.alarmCode;
        record.outletPressure = data.outletPressure;
        record.dewPoint = data.dewPoint;
        record.regenStatus = data.regenStatus;

        qDebug() << "saveAllDeviceHistory: 添加历史记录 - deviceId=" << record.deviceId
                 << "pressure=" << record.pressure;
        appendHistory(record);
    }

    qDebug() << "saveAllDeviceHistory: 已添加" << devices.size()
             << "条记录到缓冲区，等待批量写入（100条或5秒定时器触发）";
    return devices.size();
}

void DataStore::appendAlarm(const AlarmRecord& record) {
    QMetaObject::invokeMethod(m_worker, "appendAlarm", Qt::QueuedConnection,
                              Q_ARG(AlarmRecord, record));
}

void DataStore::appendAudit(const AuditRecord& record) {
    QMetaObject::invokeMethod(m_worker, "appendAudit", Qt::QueuedConnection,
                              Q_ARG(AuditRecord, record));
}

bool DataStore::insertCommLog(qint64 timestamp, const QString& logType, const QString& direction, const QString& deviceId, const QString& content) {
    return DatabaseManager::instance().insertCommLog(timestamp, logType, direction, deviceId, content);
}

QVector<CommLogEntry> DataStore::getCommLogs(qint64 startTimestamp,
                                             qint64 endTimestamp,
                                             int limit) {
    return DatabaseManager::instance().getCommLogs(startTimestamp, endTimestamp, limit);
}

QMap<QString, DeviceData> DataStore::currentData() const {
    QMutexLocker locker(&m_dataMutex);
    return m_cachedCurrentData;
}

void DataStore::updateCurrentData(const QString& deviceId, const DeviceData& data) {
    QMetaObject::invokeMethod(m_worker, "updateCurrentData", Qt::QueuedConnection,
                              Q_ARG(QString, deviceId),
                              Q_ARG(DeviceData, data));
}

}
