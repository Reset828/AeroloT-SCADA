#ifndef DATASTORE_RECORDS_H
#define DATASTORE_RECORDS_H

#include <cstdint>
#include <QString>
#include <QMetaType>
#include "../core/types.h"
#include "../core/history_record.h"

namespace DataStore {

using HistoryRecord = core::HistoryRecord;

struct AlarmRecord {
    int64_t timestamp{0};
    int deviceId{0};
    core::DeviceType deviceType{core::DeviceType::Compressor};
    int alarmType{0};
    core::AlarmSeverity alarmLevel{core::AlarmSeverity::Info};
    double value{0.0};
    double threshold{0.0};
    QString message;
    int64_t ackTime{0};
    QString ackUser;
};

struct AlarmLogEntry {
    int64_t timestamp{0};
    int deviceId{0};
    int deviceType{0};
    int alarmType{0};
    int alarmLevel{0};
    double value{0.0};
    double threshold{0.0};
    QString message;
    QString alarmTime;
};

struct CommLogEntry {
    int64_t timestamp{0};
    QString logType;
    QString direction;
    QString deviceId;
    QString content;
};

enum class AlarmInsertStatus {
    Inserted,
    ConnectionFailed,
    InsertFailed
};

struct AuditRecord {
    int64_t timestamp{0};
    QString userName;
    int operationType{0};
    int targetDeviceId{0};
    QString targetParameter;
    QString oldValue;
    QString newValue;
    int result{0};
    QString errorMessage;
};

}

Q_DECLARE_METATYPE(DataStore::HistoryRecord)
Q_DECLARE_METATYPE(DataStore::AlarmRecord)
Q_DECLARE_METATYPE(DataStore::AlarmLogEntry)
Q_DECLARE_METATYPE(DataStore::CommLogEntry)
Q_DECLARE_METATYPE(DataStore::AuditRecord)

#endif
