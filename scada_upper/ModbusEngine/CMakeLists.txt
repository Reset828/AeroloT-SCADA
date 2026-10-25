cmake_minimum_required(VERSION 3.16)
project(ModbusEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core SerialBus)

add_library(ModbusEngine STATIC
    modbus_engine.h
    modbus_engine.cpp
    modbus_worker.h
    modbus_worker.cpp
    command_queue.h
    command_queue.cpp
    register_map.h
    register_map.cpp
    scan_scheduler.h
    scan_scheduler.cpp
    modbus_session.h
    modbus_session.cpp
)

target_link_libraries(ModbusEngine PUBLIC
    Qt6::Core
    Qt6::SerialBus
    core
)

target_include_directories(ModbusEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
