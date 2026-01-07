cmake_minimum_required(VERSION 3.14)
project(rest_server LANGUAGES CXX)

find_package(nlohmann_json REQUIRED CONFIG)
find_package(httplib REQUIRED CONFIG)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json logger_lib tomlplusplus::tomlplusplus quill::quill httplib::httplib)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
