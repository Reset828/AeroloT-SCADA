#ifndef CORE_ALARM_RECORD_H
#define CORE_ALARM_RECORD_H

#include <QString>

namespace core {

struct AlarmRecord {
    qint64 timestamp{0};
    int deviceId{0};
    int deviceType{0};
    int alarmType{0};
    int alarmLevel{0};
    double value{0.0};
    double threshold{0.0};
    QString message;
    QString alarmTime;
    bool isTriggered{false};
};

}

#endif
