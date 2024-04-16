cmake_minimum_required(VERSION 3.27.0)
project(MobaGameServer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)

# 複製lib
function(copy_library build_dir lib_name)
    if(APPLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/build/3rd/${build_dir}/${lib_name}.dylib"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    elseif(UNIX)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/build/3rd/${build_dir}/${lib_name}.so"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    elseif(WIN32)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/build/3rd/${build_dir}/${lib_name}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endif()
endfunction()

file(GLOB_RECURSE SOURCES "netbus/*.cpp" "netbus/*.cc" "netbus/*.c")

add_executable(${PROJECT_NAME}  ${SOURCES}
                                ${MJSON_SOURCES} 
                                ${CRYPTO_SOURCES} 
                                ${HTTP_PARSER_SOURCES}
                                ${PROTO_SRCS}
                                ${PROTO_HDRS})

# libuv
add_subdirectory(3rd/libuv)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rd/libuv/include)

target_link_libraries(${PROJECT_NAME} PRIVATE uv)

copy_library(libuv libuv)

# hiredis
add_subdirectory(3rd/hiredis)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rd/hiredis)

target_link_libraries(${PROJECT_NAME} PRIVATE hiredis)

copy_library(hiredis libhiredisd)

# mjson
include_directories("${PROJECT_SOURCE_DIR}/3rd/mjson")

file(GLOB_RECURSE MJSON_SOURCES "3rd/mjson/*.c")
target_sources(${PROJECT_NAME} PRIVATE ${MJSON_SOURCES})

# crypto
include_directories("${PROJECT_SOURCE_DIR}/3rd/crypto")

file(GLOB_RECURSE CRYPTO_SOURCES "3rd/crypto/*.c")
target_sources(${PROJECT_NAME} PRIVATE ${CRYPTO_SOURCES})

# http parser
include_directories("${PROJECT_SOURCE_DIR}/3rd/http_parser")

file(GLOB_RECURSE HTTP_PARSER_SOURCES "3rd/http_parser/*.c")
target_sources(${PROJECT_NAME} PRIVATE ${HTTP_PARSER_SOURCES})

# mysql connector
include_directories("${PROJECT_SOURCE_DIR}/3rd/mysql/include")

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/build/libmysql.dll)
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/build/libmysqlclient.a)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/build/libmysqlservices.a)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/build/libssl.1.1.dylib)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/build/libcrypto.1.1.dylib)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/build/libmysql.so)
endif()

# protobuf
add_subdirectory(3rd/protobuf)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rd/protobuf/src)

target_link_libraries(${PROJECT_NAME} PRIVATE libprotobuf)

copy_library(protobuf libprotobufd)
