cmake_minimum_required(VERSION 3.16)
project(WebSocketServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets)

add_library(WebSocketServer STATIC
    websocket_server.h
    websocket_server.cpp
)

target_link_libraries(WebSocketServer PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
    core
)

target_include_directories(WebSocketServer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)