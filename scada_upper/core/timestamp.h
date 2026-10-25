#ifndef CORE_TIMESTAMP_H
#define CORE_TIMESTAMP_H

#include <cstdint>
#include <chrono>

namespace core {

class Timestamp {
public:
    static int64_t currentMicros() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    static int64_t currentMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    static int64_t currentSeconds() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
};

}

#endif
