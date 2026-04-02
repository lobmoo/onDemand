cmake_minimum_required(VERSION 3.16)
project(test_out LANGUAGES CXX)

file(GLOB_RECURSE API_TEST_SOURCES
    ""
    "${CMAKE_CURRENT_SOURCE_DIR}/../test*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../*_test.cc"
)

# 根据 DDS 实现选择 ondemand 所需源文件与库
if(DDS_IMPL STREQUAL "FASTDDS")
    set(IDL_GEN_DIR "${CMAKE_SOURCE_DIR}/core/ondemand/.gensrc")
    set(DDS_WRAPPER_SOURCES
        "${CMAKE_SOURCE_DIR}/common/fastdds_wrapper/fastdds_node.cc"
        "${CMAKE_SOURCE_DIR}/common/fastdds_wrapper/fastdds_qos_config.cc"
    )
    set(DDS_LIBS
        "${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfastdds.so"
        "${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfastcdr.so"
        "${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfoonathan_memory-0.7.3.so"
    )
    set(DDS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/include")
elseif(DDS_IMPL STREQUAL "TXDDS")
    set(IDL_GEN_DIR "${CMAKE_SOURCE_DIR}/core/ondemand/.gensrctx")
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
    set(DDS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/txdds/include")
else()
    message(FATAL_ERROR "Invalid DDS_IMPL for tests: ${DDS_IMPL}")
endif()

file(GLOB DDS_GEN_SOURCES_CPP "${CMAKE_SOURCE_DIR}/tests/fastdds_wrapper_TEST/*.cxx")
file(GLOB ONDEMAND_IDL_SOURCES "${IDL_GEN_DIR}/*.cxx")

set(ONDEMAND_SOURCES
    "${CMAKE_SOURCE_DIR}/core/ondemand/on_demand_pub.cc"
    "${CMAKE_SOURCE_DIR}/core/ondemand/on_demand_sub.cc"
)

set(API_TEST_INCLUDE
    "${CMAKE_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/core"
    "${CMAKE_SOURCE_DIR}/core/ondemand"
    "${IDL_GEN_DIR}"
    "${CMAKE_SOURCE_DIR}/thirdparty/nlohmann/include"
    "${CMAKE_SOURCE_DIR}/thirdparty/CRoaring/include"
    "${DDS_INCLUDE_DIR}"
)

add_executable(${PROJECT_NAME}
    ${API_TEST_SOURCES}
    ${DDS_WRAPPER_SOURCES}
    ${DDS_GEN_SOURCES_CPP}
    ${ONDEMAND_SOURCES}
    ${ONDEMAND_IDL_SOURCES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

target_include_directories(${PROJECT_NAME} PRIVATE ${API_TEST_INCLUDE})

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${DDS_LIBS}
    "${CMAKE_SOURCE_DIR}/thirdparty/CRoaring/lib/libroaring.so"
    log::log
    gtest
    gtest_main
    pthread
    rt
)

target_compile_options(${PROJECT_NAME} PRIVATE -O2)
