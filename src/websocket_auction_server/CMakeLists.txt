cmake_minimum_required(VERSION 3.14)
project(websocket_auction_server LANGUAGES CXX)

find_package(nlohmann_json REQUIRED CONFIG)
find_package(Boost REQUIRED CONFIG)

set(CPP_FILES
  main.cpp
  server.cpp
  session.cpp
  auction_manager.cpp
)

add_executable(${PROJECT_NAME} ${CPP_FILES})

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    boost::boost
    nlohmann_json::nlohmann_json
    logger_lib
    tomlplusplus::tomlplusplus
    quill::quill
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()

configure_file(cfg/cfg.toml ${CMAKE_CURRENT_BINARY_DIR}/cfg.toml COPYONLY)
