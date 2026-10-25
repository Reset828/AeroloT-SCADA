#ifndef MODBUSENGINE_REGISTERMAP_H
#define MODBUSENGINE_REGISTERMAP_H

#include <vector>
#include <QMap>
#include <QString>
#include "core/types.h"

namespace ModbusEngine {

struct ReadRequest {
    int slaveId;
    int startAddress;
    int quantity;
    core::ModbusRegisterType registerType;
};

class RegisterMap {
public:
    static QMap<int, QMap<int, QString>> getRegisterNames(int slaveId);
    static std::vector<ReadRequest> buildReadGroups(int slaveId);

private:
    static QMap<int, QMap<int, QString>>& getMap(int slaveId);
};

}
#endif
