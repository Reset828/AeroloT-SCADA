#ifndef CORE_HISTORY_RECORD_H
#define CORE_HISTORY_RECORD_H

#include <QString>

#include "types.h"

namespace core {

struct HistoryRecord {
    int64_t timestamp{0};
    QString historyTime;
    int deviceId{0};
    DeviceType deviceType{DeviceType::Compressor};
    double pressure{0.0};
    double temperature{0.0};
    int status{0};
    double runtimeHours{0.0};
    double vfdFrequency{0.0};
    double phaseCurrent{0.0};
    int alarmCode{0};
    double outletPressure{0.0};
    double dewPoint{0.0};
    int regenStatus{0};
};

}

#endif
