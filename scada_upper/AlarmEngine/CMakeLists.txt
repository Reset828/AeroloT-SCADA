cmake_minimum_required(VERSION 3.16)
project(AlarmEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

add_library(AlarmEngine STATIC
    alarm_evaluator.h
    alarm_evaluator.cpp
    alarm_config_manager.h
    alarm_config_manager.cpp
    alarm_manager.h
    alarm_manager.cpp
    webhook_client.h
    webhook_client.cpp
)

target_link_libraries(AlarmEngine PUBLIC
    Application
    core
    Qt6::Core
    Qt6::Network
)

target_include_directories(AlarmEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
