project("soap_server")

find_package(pugixml REQUIRED CONFIG)
find_package(Boost REQUIRED CONFIG)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CPP_FILES main.cpp soap_server.cpp task_manager.cpp task.cpp)

add_executable (${PROJECT_NAME} ${CPP_FILES})

target_link_libraries (${PROJECT_NAME} boost::boost logger_lib tomlplusplus::tomlplusplus quill::quill pugixml::pugixml)

if(MSVC)
    if(SANITIZER)
        target_compile_options(${PROJECT_NAME} PRIVATE /fsanitize=address /Zi)
        target_link_options(${PROJECT_NAME} PRIVATE /fsanitize=address)
    endif()
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()
