find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(device_data_mapper_tests
    device_data_mapper_tests.cpp
)
target_link_libraries(device_data_mapper_tests PRIVATE
    Application
    Qt6::Test
)
add_test(NAME device_data_mapper_tests COMMAND device_data_mapper_tests)

add_executable(alarm_evaluator_tests
    alarm_evaluator_tests.cpp
)

target_link_libraries(alarm_evaluator_tests PRIVATE
    AlarmEngine
    Qt6::Test
)

add_test(NAME alarm_evaluator_tests COMMAND alarm_evaluator_tests)

add_executable(alarm_config_manager_tests
    alarm_config_manager_tests.cpp
)
target_link_libraries(alarm_config_manager_tests PRIVATE
    AlarmEngine
    Qt6::Test
)
add_test(NAME alarm_config_manager_tests COMMAND alarm_config_manager_tests)

add_executable(command_queue_tests
    command_queue_tests.cpp
)
target_link_libraries(command_queue_tests PRIVATE
    ModbusEngine
    Qt6::Test
)
add_test(NAME command_queue_tests COMMAND command_queue_tests)

add_executable(register_map_tests
    register_map_tests.cpp
)
target_link_libraries(register_map_tests PRIVATE
    ModbusEngine
    Qt6::Test
)
add_test(NAME register_map_tests COMMAND register_map_tests)

add_executable(scan_scheduler_tests
    scan_scheduler_tests.cpp
)
target_link_libraries(scan_scheduler_tests PRIVATE
    ModbusEngine
    Qt6::Test
)
add_test(NAME scan_scheduler_tests COMMAND scan_scheduler_tests)

add_executable(modbus_worker_tests
    modbus_worker_tests.cpp
)
target_link_libraries(modbus_worker_tests PRIVATE
    ModbusEngine
    Qt6::Test
    Qt6::Network
)
add_test(NAME modbus_worker_tests COMMAND modbus_worker_tests)

add_executable(history_widget_tests
    history_widget_tests.cpp
)
target_link_libraries(history_widget_tests PRIVATE
    UI
    Qt6::Test
)
add_test(NAME history_widget_tests COMMAND history_widget_tests)

add_executable(trend_chart_tests
    trend_chart_tests.cpp
)
target_link_libraries(trend_chart_tests PRIVATE
    UI
    Qt6::Test
)
add_test(NAME trend_chart_tests COMMAND trend_chart_tests)
set_tests_properties(trend_chart_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(datastore_tests
    datastore_tests.cpp
)
target_link_libraries(datastore_tests PRIVATE
    DataStore
    Qt6::Test
)
add_test(NAME datastore_tests COMMAND datastore_tests)

add_executable(alarm_manager_tests
    alarm_manager_tests.cpp
)
target_link_libraries(alarm_manager_tests PRIVATE
    AlarmEngine
    DataStore
    Qt6::Test
)
add_test(NAME alarm_manager_tests COMMAND alarm_manager_tests)

add_executable(alarm_log_reader_tests
    alarm_log_reader_tests.cpp
)
target_link_libraries(alarm_log_reader_tests PRIVATE
    DataStore
    Qt6::Test
)
add_test(NAME alarm_log_reader_tests COMMAND alarm_log_reader_tests)

add_executable(websocket_server_tests
    websocket_server_tests.cpp
)
target_link_libraries(websocket_server_tests PRIVATE
    WebSocketServer
    Qt6::Test
)
add_test(NAME websocket_server_tests COMMAND websocket_server_tests)

add_executable(webhook_client_tests
    webhook_client_tests.cpp
)
target_link_libraries(webhook_client_tests PRIVATE
    AlarmEngine
    Qt6::Test
)
add_test(NAME webhook_client_tests COMMAND webhook_client_tests)
