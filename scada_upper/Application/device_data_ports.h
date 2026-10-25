#ifndef APPLICATION_DEVICE_DATA_PORTS_H
#define APPLICATION_DEVICE_DATA_PORTS_H

#include <QString>

#include "core/device_data.h"

namespace Application {

class CurrentDeviceDataWriter {
public:
    virtual ~CurrentDeviceDataWriter() = default;
    virtual void updateCurrentData(const QString& deviceKey,
                                   const core::DeviceData& data) = 0;
};

class DeviceAlarmProcessor {
public:
    virtual ~DeviceAlarmProcessor() = default;
    virtual void processDeviceData(const core::DeviceData& data) = 0;
};

class DeviceDataPublisher {
public:
    virtual ~DeviceDataPublisher() = default;
    virtual void broadcastDeviceData(const QString& deviceKey,
                                     const core::DeviceData& data) = 0;
};

}

#endif
