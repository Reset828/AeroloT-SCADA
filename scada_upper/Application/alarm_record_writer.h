#ifndef APPLICATION_ALARM_RECORD_WRITER_H
#define APPLICATION_ALARM_RECORD_WRITER_H

#include "core/alarm_record.h"

namespace Application {

enum class AlarmRecordWriteStatus {
    ConnectionFailed,
    InsertFailed,
    Inserted
};

class AlarmRecordWriter {
public:
    virtual ~AlarmRecordWriter() = default;
    virtual AlarmRecordWriteStatus writeAlarmRecord(const core::AlarmRecord& record) = 0;
};

}

#endif
