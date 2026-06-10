cmake_minimum_required(VERSION 3.14)
project(OrderManagementSystem)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json REQUIRED CONFIG)
find_package(modern-cpp-kafka REQUIRED CONFIG)

# Включаем директории
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Общие исходники для всех целей
set(COMMON_SOURCES
    src/models.cpp
)

# Отдельные исходники для сервисов
set(PRODUCER_SOURCES
    src/order_producer.cpp
    src/order_service.cpp
)

set(CONSUMER_SOURCES
    src/order_consumer.cpp
)

set(NOTIFICATION_CONSUMER_SOURCES
    src/notification_consumer.cpp
)

set(AUDIT_CONSUMER_SOURCES
    src/audit_consumer.cpp
)

# Базовые библиотеки для линковки
set(COMMON_LIBS
    modern-cpp-kafka::modern-cpp-kafka
    nlohmann_json::nlohmann_json
)

# ============================================
# ЦЕЛЬ 1: Producer сервис (создание заказов)
# ============================================
add_executable(order_producer ${COMMON_SOURCES} ${PRODUCER_SOURCES})
target_link_libraries(order_producer ${COMMON_LIBS})

# Определяем макрос для producer
target_compile_definitions(order_producer PRIVATE -DIS_PRODUCER)

# Устанавливаем выходную директорию
set_target_properties(order_producer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ============================================
# ЦЕЛЬ 2: Consumer сервис (обработка заказов)
# ============================================
add_executable(order_consumer ${COMMON_SOURCES} ${CONSUMER_SOURCES})
target_link_libraries(order_consumer ${COMMON_LIBS})

target_compile_definitions(order_consumer PRIVATE -IS_CONSUMER)

set_target_properties(order_consumer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)


# ============================================
# ЦЕЛЬ 3: Notification Consumer (уведомления)
# ============================================
add_executable(notification_consumer ${COMMON_SOURCES} ${NOTIFICATION_CONSUMER_SOURCES})
target_link_libraries(notification_consumer ${COMMON_LIBS})

set_target_properties(notification_consumer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)


# ============================================
# ЦЕЛЬ 4: Audit Consumer (аудит)
# ============================================
add_executable(audit_consumer ${COMMON_SOURCES} ${AUDIT_CONSUMER_SOURCES})
target_link_libraries(audit_consumer ${COMMON_LIBS})

set_target_properties(audit_consumer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)


# ============================================
# Дополнительная цель: все сервисы вместе
# ============================================
add_custom_target(run-all-services
    COMMAND ${CMAKE_COMMAND} -E echo "Starting all services..."
    COMMAND ${CMAKE_COMMAND} -E echo "Run each service separately in different terminals:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./bin/order_producer"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./bin/order_consumer"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./bin/notification_consumer"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./bin/audit_consumer"
    COMMENT "Instructions for running all services"
)

# Выводим информацию о сборке
message(STATUS "========================================")
message(STATUS "Order Management System Configuration:")
message(STATUS "  Producer: ${BUILD_PRODUCER}")
message(STATUS "  Order Consumer: ${BUILD_CONSUMER}")
message(STATUS "  Notification Consumer: ${BUILD_NOTIFICATION_CONSUMER}")
message(STATUS "  Audit Consumer: ${BUILD_AUDIT_CONSUMER}")
message(STATUS "========================================")