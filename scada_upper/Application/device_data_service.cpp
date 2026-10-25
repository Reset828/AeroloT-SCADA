#include "device_data_service.h"

namespace Application {

DeviceDataService::DeviceDataService(CurrentDeviceDataWriter& dataWriter,
                                     DeviceAlarmProcessor& alarmProcessor,
                                     DeviceDataPublisher& publisher)
    : m_dataWriter(dataWriter)
    , m_alarmProcessor(alarmProcessor)
    , m_publisher(publisher)
{
}

std::optional<MappedDeviceData> DeviceDataService::processModbusRegisters(
    int slaveId, const QVector<quint16>& values, qint64 timestamp)
{
    auto mapped = DeviceDataMapper::fromModbusRegisters(slaveId, values, timestamp);
    if (!mapped) {
        return std::nullopt;
    }

    m_dataWriter.updateCurrentData(mapped->deviceKey, mapped->data);
    m_publisher.broadcastDeviceData(mapped->deviceKey, mapped->data);
    m_alarmProcessor.processDeviceData(mapped->data);
    return mapped;
}

}
