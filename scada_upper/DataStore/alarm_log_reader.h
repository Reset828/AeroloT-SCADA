#ifndef DATASTORE_ALARM_LOG_READER_H
#define DATASTORE_ALARM_LOG_READER_H

#include <QVector>
#include <QString>

#include "records.h"

namespace DataStore {

enum class AlarmLogReadFailure {
    None,
    Open,
    Prepare,
    Execute
};

struct AlarmLogReadResult {
    QVector<AlarmLogEntry> records;
    AlarmLogReadFailure failure{AlarmLogReadFailure::None};
    QString error;
};

class AlarmLogReader {
public:
    static AlarmLogReadResult read(const QString& dbPath, qint64 startMs,
                                   qint64 endMs);
    static void closeCurrentThreadConnection();
};

}

#endif
