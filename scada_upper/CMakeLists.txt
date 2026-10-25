cmake_minimum_required(VERSION 3.16)
project(AeroloT_SCADA)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core SerialBus Sql Network WebSockets Widgets PrintSupport Multimedia)

add_subdirectory(core)
add_subdirectory(Application)
add_subdirectory(ModbusEngine)
add_subdirectory(DataStore)
add_subdirectory(AlarmEngine)
add_subdirectory(WebSocketServer)
add_subdirectory(UI)

# Optional test suites
option(BUILD_TESTS "Build test targets" OFF)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# 主应用程序可执行文件
add_executable(AeroloT_SCADA
    main.cpp
)

# 设置输出目录为build/Debug
set_target_properties(AeroloT_SCADA PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
)

target_link_libraries(AeroloT_SCADA PRIVATE
    core
    ModbusEngine
    DataStore
    AlarmEngine
    WebSocketServer
    UI
    Qt6::Core
    Qt6::SerialBus
    Qt6::Sql
    Qt6::Network
    Qt6::WebSockets
    Qt6::Widgets
    Qt6::PrintSupport
)

target_include_directories(AeroloT_SCADA PRIVATE
    ${CMAKE_SOURCE_DIR}
)
