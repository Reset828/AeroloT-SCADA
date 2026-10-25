cmake_minimum_required(VERSION 3.16)
project(DataStore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql)

add_library(DataStore STATIC
    database_manager.h
    database_manager.cpp
    history_data_reader.h
    history_data_reader.cpp
    alarm_log_reader.h
    alarm_log_reader.cpp
    records.h
    datastore.h
    datastore.cpp
)

target_link_libraries(DataStore PUBLIC
    Application
    Qt6::Core
    Qt6::Sql
    core
)

target_include_directories(DataStore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
