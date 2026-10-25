#ifndef APPLICATION_DEVICE_DATA_MAPPER_H
#define APPLICATION_DEVICE_DATA_MAPPER_H

#include <QVector>
#include <optional>
#include "core/device_data.h"

namespace Application {

struct MappedDeviceData {
    QString deviceKey;
    core::DeviceData data;
};

class DeviceDataMapper
{
public:
    static std::optional<MappedDeviceData> fromModbusRegisters(
        int slaveId, const QVector<quint16>& values, qint64 timestamp);
};

}

#endif
