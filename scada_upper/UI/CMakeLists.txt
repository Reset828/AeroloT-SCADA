cmake_minimum_required(VERSION 3.16)
project(UI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia SerialBus WebSockets Network Concurrent PrintSupport)

add_library(UI STATIC
    device_widget.h
    device_widget.cpp
    main_window.h
    main_window.cpp
    trend_chart.h
    trend_chart.cpp
    alarm_widget.h
    alarm_widget.cpp
    navigation_bar.h
    navigation_bar.cpp
    realtime_widget.h
    realtime_widget.cpp
    alarm_record_widget.h
    alarm_record_widget.cpp
    history_widget.h
    history_widget.cpp
    commlog_widget.h
    commlog_widget.cpp
    ${CMAKE_SOURCE_DIR}/third_party/qcustomplot.h
    ${CMAKE_SOURCE_DIR}/third_party/qcustomplot.cpp
    titlebar.h
    titlebar.cpp
)

target_link_libraries(UI PUBLIC
    core
    Application
    DataStore
    AlarmEngine
    ModbusEngine
    WebSocketServer
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::SerialBus
    Qt6::WebSockets
    Qt6::Network
    Qt6::Concurrent
    Qt6::PrintSupport
)

target_include_directories(UI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party
)
