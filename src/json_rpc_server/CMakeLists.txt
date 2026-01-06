project("json_rpc_server")

find_package(nlohmann_json REQUIRED CONFIG)
find_package(Boost REQUIRED CONFIG)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CPP_FILES main.cpp)

add_executable (${PROJECT_NAME} ${CPP_FILES})

target_link_libraries (${PROJECT_NAME} boost::boost nlohmann_json::nlohmann_json logger_lib tomlplusplus::tomlplusplus quill::quill)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
    target_compile_options(${PROJECT_NAME} PRIVATE /fsanitize=address /Zi)
    target_link_options(${PROJECT_NAME} PRIVATE /fsanitize=address)
endif()
