cmake_minimum_required(VERSION 3.14)
project(long_polling_chat_server LANGUAGES CXX)

find_package(nlohmann_json REQUIRED CONFIG)
find_package(httplib REQUIRED CONFIG)

add_executable(${PROJECT_NAME}
  main.cpp
  chat_manager.cpp
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    nlohmann_json::nlohmann_json
    httplib::httplib
    logger_lib
    tomlplusplus::tomlplusplus
    quill::quill
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

configure_file(cfg/cfg.toml ${CMAKE_CURRENT_BINARY_DIR}/cfg.toml COPYONLY)


