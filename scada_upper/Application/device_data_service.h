#ifndef APPLICATION_DEVICE_DATA_SERVICE_H
#define APPLICATION_DEVICE_DATA_SERVICE_H

#include <optional>
#include <QString>
#include <QVector>

#include "device_data_mapper.h"
#include "device_data_ports.h"

namespace Application {

class DeviceDataService {
public:
    DeviceDataService(CurrentDeviceDataWriter& dataWriter,
                      DeviceAlarmProcessor& alarmProcessor,
                      DeviceDataPublisher& publisher);

    std::optional<MappedDeviceData> processModbusRegisters(
        int slaveId, const QVector<quint16>& values, qint64 timestamp);

private:
    CurrentDeviceDataWriter& m_dataWriter;
    DeviceAlarmProcessor& m_alarmProcessor;
    DeviceDataPublisher& m_publisher;
};

}

#endif
