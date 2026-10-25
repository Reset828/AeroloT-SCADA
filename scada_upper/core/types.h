#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <cstdint>

namespace core {

enum class ModbusRegisterType : uint8_t {
    Coil = 0,
    Discrete = 1,
    Holding = 2,
    Input = 3
};

enum class DeviceType : uint8_t {
    Compressor = 0,
    Tank = 1,
    Dryer = 2
};

enum class AlarmSeverity : uint8_t {
    Info = 0,
    Warning = 1,
    Critical = 2
};

enum class ConnectionState : uint8_t {
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    Reconnecting = 3
};

enum class DeviceStatus : uint8_t {
    Stopped = 0,
    Running = 1,
    Fault = 2
};

}

#endif
