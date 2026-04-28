cmake_minimum_required(VERSION 3.16)

# 根据 DDS 实现选择源文件与库
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

find_package(GTest REQUIRED)

file(GLOB DDS_GEN_SOURCES_CPP "${CMAKE_SOURCE_DIR}/tests/fastdds_wrapper_TEST/*.cxx")
file(GLOB ONDEMAND_IDL_SOURCES "${IDL_GEN_DIR}/*.cxx")

set(ONDEMAND_SOURCES
    "${CMAKE_SOURCE_DIR}/core/ondemand/on_demand_pub.cc"
    "${CMAKE_SOURCE_DIR}/core/ondemand/on_demand_sub.cc"
)

set(COMMON_INCLUDE
    "${CMAKE_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/core"
    "${CMAKE_SOURCE_DIR}/core/ondemand"
    "${IDL_GEN_DIR}"
    "${CMAKE_SOURCE_DIR}/thirdparty/nlohmann/include"
    "${CMAKE_SOURCE_DIR}/thirdparty/CRoaring/include"
    "${DDS_INCLUDE_DIR}"
)

set(COMMON_LIBS
    ${DDS_LIBS}
    "${CMAKE_SOURCE_DIR}/thirdparty/CRoaring/lib/libroaring.so"
    log::log
    GTest::gtest
    GTest::gtest_main
    pthread
    rt
)

# ── test_log ──────────────────────────────────────────────────────────────────
add_executable(test_log
    "${CMAKE_SOURCE_DIR}/common/log/logger_test.cc"
    "${CMAKE_SOURCE_DIR}/common/timer_wheel/timer_scheduler_test.cc"
)
set_target_properties(test_log PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
target_include_directories(test_log PRIVATE ${COMMON_INCLUDE})
target_link_libraries(test_log PRIVATE ${COMMON_LIBS})
target_compile_options(test_log PRIVATE -O2)

# ── test_ddswrapper ───────────────────────────────────────────────────────────
add_executable(test_ddswrapper
    "${CMAKE_SOURCE_DIR}/tests/fastdds_node_test.cc"
    ${DDS_WRAPPER_SOURCES}
    ${DDS_GEN_SOURCES_CPP}
)
set_target_properties(test_ddswrapper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
target_include_directories(test_ddswrapper PRIVATE ${COMMON_INCLUDE})
target_link_libraries(test_ddswrapper PRIVATE ${COMMON_LIBS})
target_compile_options(test_ddswrapper PRIVATE -O2)

# ── test_ondemand ─────────────────────────────────────────────────────────────
add_executable(test_ondemand
    "${CMAKE_SOURCE_DIR}/core/ondemand/ondemand_pubsub_test.cc"
    ${DDS_WRAPPER_SOURCES}
    ${DDS_GEN_SOURCES_CPP}
    ${ONDEMAND_SOURCES}
    ${ONDEMAND_IDL_SOURCES}
)
set_target_properties(test_ondemand PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
target_include_directories(test_ondemand PRIVATE ${COMMON_INCLUDE})
target_link_libraries(test_ondemand PRIVATE ${COMMON_LIBS})
target_compile_options(test_ondemand PRIVATE -O2)
