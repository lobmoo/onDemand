cmake_minimum_required(VERSION 3.16)

project(node_example_exec  LANGUAGES CXX)

if(DDS_IMPL STREQUAL "FASTDDS")
    set(HELLO_WORLD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldOneFast/source")
    set(DDS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/include")
    set(DDS_WRAPPER_SOURCES
        "${CMAKE_SOURCE_DIR}/common/fastdds_wrapper/fastdds_node.cc"
        "${CMAKE_SOURCE_DIR}/common/fastdds_wrapper/fastdds_qos_config.cc"
    )
    set(DDS_LIBS
        "${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfastdds.so"
        "${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfastcdr.so"
        "${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfoonathan_memory-0.7.3.so"
    )
elseif(DDS_IMPL STREQUAL "TXDDS")
    set(HELLO_WORLD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldOneTx/source")
    set(DDS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/txdds/include")
    set(DDS_WRAPPER_SOURCES
        "${CMAKE_SOURCE_DIR}/common/txdds_wrapper/txdds_node.cc"
        "${CMAKE_SOURCE_DIR}/common/txdds_wrapper/txdds_qos_config.cc"
    )
    set(DDS_LIBS
        "${CMAKE_SOURCE_DIR}/thirdparty/txdds/lib/libTXDDS.so"
        "${CMAKE_SOURCE_DIR}/thirdparty/txdds/lib/libTXCDR.so"
        "${CMAKE_SOURCE_DIR}/thirdparty/txdds/lib/libTXRuntimeLog.so"
        "${CMAKE_SOURCE_DIR}/thirdparty/txdds/lib/libssl.so"
        "${CMAKE_SOURCE_DIR}/thirdparty/txdds/lib/libcrypto.so"
    )
else()
    message(FATAL_ERROR "Invalid DDS_IMPL: ${DDS_IMPL}")
endif()

file(GLOB DDS_SHM_HELLOWORLD_EXAMPLE_SOURCES
    "${HELLO_WORLD_SOURCE_DIR}/*.cxx"
    "${HELLO_WORLD_SOURCE_DIR}/*.cpp"
    "${HELLO_WORLD_SOURCE_DIR}/*.ipp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(${PROJECT_NAME} ${DDS_SHM_HELLOWORLD_EXAMPLE_SOURCES} ${DDS_WRAPPER_SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sample")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${HELLO_WORLD_SOURCE_DIR}
    ${DDS_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${DDS_LIBS}
    log::log
    pthread
)

set(CMAKE_INSTALL_RPATH "${DDS_INCLUDE_DIR}/../lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

target_compile_options(${PROJECT_NAME} PRIVATE -g)
