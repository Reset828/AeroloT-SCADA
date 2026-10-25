#include "register_map.h"
#include <algorithm>
#include <QList>

namespace ModbusEngine {

static QMap<int, QMap<int, QString>> g_compressor1Map;
static QMap<int, QMap<int, QString>> g_compressor2Map;
static QMap<int, QMap<int, QString>> g_compressor3Map;
static QMap<int, QMap<int, QString>> g_tankMap;
static QMap<int, QMap<int, QString>> g_dryerMap;
static bool g_initialized = false;

static void ensureInitialized() {
    if (g_initialized)
        return;

    g_compressor1Map[0] = {{0, "discharge_pressure"},
                           {1, "discharge_temp"},
                           {2, "run_status"},
                           {3, "load_unload"},
                           {4, "phase_current"},
                           {5, "alarm_code"}};

    g_compressor2Map = g_compressor1Map;
    g_compressor3Map = g_compressor1Map;

    g_tankMap[0] = {{0, "tank_pressure"},
                    {1, "tank_temp"},
                    {2, "tank_status"}};

    g_dryerMap[0] = {{0, "inlet_pressure"},
                     {1, "outlet_pressure"},
                     {2, "run_status"},
                     {3, "dew_point"},
                     {4, "regen_status"}};

    g_initialized = true;
}

QMap<int, QMap<int, QString>>& RegisterMap::getMap(int slaveId) {
    ensureInitialized();
    switch (slaveId) {
    case 1: return g_compressor1Map;
    case 2: return g_compressor2Map;
    case 3: return g_compressor3Map;
    case 4: return g_tankMap;
    case 5: return g_dryerMap;
    default: return g_compressor1Map;
    }
}

QMap<int, QMap<int, QString>> RegisterMap::getRegisterNames(int slaveId) {
    return getMap(slaveId);
}

std::vector<ReadRequest> RegisterMap::buildReadGroups(int slaveId) {
    std::vector<ReadRequest> groups;
    QMap<int, QMap<int, QString>> regMap = getMap(slaveId);

    QList<int> baseAddresses = regMap.keys();
    std::sort(baseAddresses.begin(), baseAddresses.end());

    for (int baseAddr : baseAddresses) {
        QMap<int, QString> regs = regMap[baseAddr];
        int count = regs.size();
        core::ModbusRegisterType regType;

        // 根据基地址判断寄存器类型
        // 地址0：保持寄存器（可读写）
        // 地址30001：输入寄存器（只读镜像）
        if (baseAddr >= 30000) {
            regType = core::ModbusRegisterType::Input;
        } else {
            regType = core::ModbusRegisterType::Holding;
        }

        bool merged = false;
        for (auto& group : groups) {
            if (group.registerType == regType &&
                group.startAddress + group.quantity == baseAddr) {
                group.quantity += count;
                merged = true;
                break;
            }
        }

        if (!merged) {
            ReadRequest req;
            req.slaveId = slaveId;
            req.startAddress = baseAddr;
            req.quantity = count;
            req.registerType = regType;
            groups.push_back(req);
        }
    }

    return groups;
}

}
