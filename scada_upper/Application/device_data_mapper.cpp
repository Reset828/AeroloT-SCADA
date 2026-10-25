#include "device_data_mapper.h"

namespace Application {

std::optional<MappedDeviceData> DeviceDataMapper::fromModbusRegisters(
    int slaveId, const QVector<quint16>& values, qint64 timestamp)
{
    if (values.isEmpty())
        return std::nullopt;

    MappedDeviceData mapped;
    mapped.data.timestamp = timestamp;
    mapped.data.deviceId = slaveId;

    if (slaveId >= 1 && slaveId <= 3) {
        mapped.deviceKey = QStringLiteral("COM-00%1").arg(slaveId);
        mapped.data.deviceType = core::DeviceType::Compressor;

        if (values.size() > 0) mapped.data.pressure = values[0] / 100.0;
        if (values.size() > 1) mapped.data.temperature = values[1] / 10.0;
        if (values.size() > 2) mapped.data.status = values[2];
        if (values.size() > 4) mapped.data.phaseCurrent = values[4] / 10.0;
        if (values.size() > 5) mapped.data.alarmCode = values[5];
    } else if (slaveId == 4) {
        mapped.deviceKey = QStringLiteral("TANK-001");
        mapped.data.deviceType = core::DeviceType::Tank;

        if (values.size() > 0) mapped.data.pressure = values[0] / 100.0;
        if (values.size() > 1) mapped.data.temperature = values[1] / 10.0;
        if (values.size() > 2) mapped.data.status = values[2];
    } else if (slaveId == 5) {
        mapped.deviceKey = QStringLiteral("DRY-001");
        mapped.data.deviceType = core::DeviceType::Dryer;

        if (values.size() > 0) mapped.data.pressure = values[0] / 100.0;
        if (values.size() > 1) mapped.data.outletPressure = values[1] / 100.0;
        if (values.size() > 2) mapped.data.status = values[2];
        if (values.size() > 3) {
            const qint16 rawDewPoint = static_cast<qint16>(values[3]);
            mapped.data.dewPoint = rawDewPoint / 10.0;
        }
        if (values.size() > 4) mapped.data.regenStatus = values[4];
    } else {
        return std::nullopt;
    }

    return mapped;
}

}
