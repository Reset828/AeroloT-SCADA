#ifndef CORE_DEVICE_DATA_H
#define CORE_DEVICE_DATA_H

#include <QMetaType>
#include "types.h"

namespace core {

struct DeviceData {
    int deviceId{0};
    DeviceType deviceType{DeviceType::Compressor};
    double pressure{0.0};
    double temperature{0.0};
    int status{0};
    double runtimeHours{0.0};
    double vfdFrequency{0.0};
    double phaseCurrent{0.0};
    int alarmCode{0};
    int64_t timestamp{0};
    double outletPressure{0.0};
    int regenStatus{0};
    double dewPoint{0.0};
};

}

Q_DECLARE_METATYPE(core::DeviceData)

#endif
