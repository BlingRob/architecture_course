if(MSVC)
    add_definitions(-DPUGIXML_USE_ANNOTATIONS=1)
endif()

find_package(tomlplusplus REQUIRED CONFIG)
find_package(quill REQUIRED CONFIG)

set(LOGGER_SRC logger/logger.cpp)

add_library(logger_lib STATIC ${LOGGER_SRC})

target_link_libraries (logger_lib PRIVATE tomlplusplus::tomlplusplus quill::quill)

# if(MSVC)
#     target_compile_options(logger_lib PRIVATE /fsanitize=address /Zi)
#     target_link_options(logger_lib PRIVATE /fsanitize=address)
# endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/logger)

# if(JSON_RPC_SERVER)
#     add_subdirectory(json_rpc_server)
# endif()

# if(SOAP_SERVER)
    add_subdirectory(soap_server)
# endif()