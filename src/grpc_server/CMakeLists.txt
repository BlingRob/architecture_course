cmake_minimum_required(VERSION 3.14)
project(grpc_server LANGUAGES CXX)

find_package(asio-grpc REQUIRED CONFIG)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE asio-grpc::asio-grpc logger_lib tomlplusplus::tomlplusplus quill::quill)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

if(${asio-grpc_VERSION} VERSION_GREATER_EQUAL 2)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_GRPC_V2)
endif()

if(NOT CMAKE_CROSSCOMPILING)
    asio_grpc_protobuf_generate(
        GENERATE_GRPC
        TARGET
        ${PROJECT_NAME}
        OUT_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
        PROTOS
        "${CMAKE_CURRENT_LIST_DIR}/ecommerce.proto")
endif()