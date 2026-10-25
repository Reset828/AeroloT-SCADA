cmake_minimum_required(VERSION 3.16)
project(Application)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(Application STATIC
    alarm_record_writer.h
    device_data_mapper.h
    device_data_mapper.cpp
    device_data_ports.h
    device_data_service.h
    device_data_service.cpp
)

target_link_libraries(Application PUBLIC
    core
    Qt6::Core
)

target_include_directories(Application PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
