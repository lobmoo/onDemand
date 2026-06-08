cmake_minimum_required(VERSION 3.16)

project(demo_exec  LANGUAGES  CXX)
set(LIBRARY_OUTPUT_NAME  demo)

# 根据 DDS 实现类型选择生成文件目录
if(DDS_IMPL STREQUAL "FASTDDS")
    set(IDL_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ondemand/.gensrc")
    message(STATUS "Using FastDDS IDL gen dir: ${IDL_GEN_DIR}")
elseif(DDS_IMPL STREQUAL "TXDDS")
    set(IDL_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ondemand/.gensrctx")
    message(STATUS "Using TXDDS IDL gen dir: ${IDL_GEN_DIR}")
else()
    message(FATAL_ERROR "Invalid DDS_IMPL: ${DDS_IMPL}")
endif()

#头文件
set(OBJ_CPP_INCLUDE
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/core
    ${IDL_GEN_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty/spdlog/include
    ${CMAKE_SOURCE_DIR}/thirdparty/CRoaring/include
)

# DDS 实现特定的头文件路径和库
if(DDS_IMPL STREQUAL "FASTDDS")
    list(APPEND OBJ_CPP_INCLUDE ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/include)

    # FastDDS wrapper源文件
    set(SOURCES_CC_FOR_DDS_WRAPPER
        ${CMAKE_SOURCE_DIR}/common/fastdds_wrapper/fastdds_node.cc
        ${CMAKE_SOURCE_DIR}/common/fastdds_wrapper/fastdds_qos_config.cc
    )

    # FastDDS 库链接
    set(DDS_LIBS
        ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfastdds.so
        ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfastcdr.so
        ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfoonathan_memory-0.7.3.so
    )

    message(STATUS "FastDDS wrapper will be compiled")

elseif(DDS_IMPL STREQUAL "TXDDS")
    list(APPEND OBJ_CPP_INCLUDE ${CMAKE_SOURCE_DIR}/thirdparty/txdds/include)

    # TXDDS wrapper源文件
    set(SOURCES_CC_FOR_DDS_WRAPPER
        ${CMAKE_SOURCE_DIR}/common/txdds_wrapper/txdds_node.cc
        ${CMAKE_SOURCE_DIR}/common/txdds_wrapper/txdds_qos_config.cc
    )

    # 直接用版本化 .so 完整路径，避免 CI 下 symlink 丢失导致链接失败
    file(GLOB TXDDS_LIB_FILES "${CMAKE_SOURCE_DIR}/thirdparty/txdds/lib/libTXDDS.so.*")
    file(GLOB TXCDR_LIB_FILES "${CMAKE_SOURCE_DIR}/thirdparty/txdds/lib/libTXCDR.so.*")
    list(GET TXDDS_LIB_FILES 0 TXDDS_LIB)
    list(GET TXCDR_LIB_FILES 0 TXCDR_LIB)
    set(DDS_LIBS
        "${TXDDS_LIB}"
        "${TXCDR_LIB}"
        "${CMAKE_SOURCE_DIR}/thirdparty/txdds/lib/libTXRuntimeLog.so"
        ssl crypto
    )

    # CI 下 git checkout 可能丢失 symlink，构建时自动创建运行时所需的 symlink
    get_filename_component(TXDDS_VER_FILE "${TXDDS_LIB}" NAME)
    get_filename_component(TXCDR_VER_FILE "${TXCDR_LIB}" NAME)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${TXDDS_VER_FILE}" "${CMAKE_SOURCE_DIR}/thirdparty/txdds/lib/libTXDDS.so")
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${TXCDR_VER_FILE}" "${CMAKE_SOURCE_DIR}/thirdparty/txdds/lib/libTXCDR.so")

    message(STATUS "TXDDS wrapper will be compiled")
else()
    message(FATAL_ERROR "Invalid DDS_IMPL: ${DDS_IMPL}")
endif()

# set(SOURCES_CC_FOR_NET_WORK
#     "${CMAKE_CURRENT_SOURCE_DIR}/net_interface.cc"
# )

file(GLOB SOURCES_CC_FOR_ONDEMAND "${CMAKE_CURRENT_SOURCE_DIR}/ondemand/*.cc")
# 测试源不应参与 demo 目标，避免 gtest 符号被拉入非测试可执行文件。
list(FILTER SOURCES_CC_FOR_ONDEMAND EXCLUDE REGEX ".*_test\\.cc$")
file(GLOB SOURCES_CC_FOR_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/main.cc")

# IDL生成的源文件（根据 IDL_GEN_DIR）
file(GLOB SOURCES_CXX_FOR_IDL "${IDL_GEN_DIR}/*.cxx")

# 合并所有查找到的文件列表
set(MAIN_SOURCES ${SOURCES_CC_FOR_MAIN} ${SOURCES_CC_FOR_ONDEMAND} ${SOURCES_CC_FOR_DDS_WRAPPER} ${SOURCES_CXX_FOR_IDL})


option(BUILD_LIBRARY "Build library" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)


if(BUILD_LIBRARY)
    add_library(${LIBRARY_OUTPUT_NAME} ${SOURCES_CC_FOR_ONDEMAND})  # 添加源 文件
    target_include_directories(${LIBRARY_OUTPUT_NAME} PRIVATE ${OBJ_CPP_INCLUDE})

    # 库也需要链接依赖
    target_link_libraries(${LIBRARY_OUTPUT_NAME} PRIVATE
        ${DDS_LIBS}
        ${CMAKE_SOURCE_DIR}/thirdparty/CRoaring/lib/libroaring.so
        pthread
        rt
        log::log
    )

    target_compile_options(${LIBRARY_OUTPUT_NAME} PRIVATE -fPIC -g)
endif()


# 添加可执行文件目标
add_executable(${PROJECT_NAME} ${MAIN_SOURCES})
#指定可执行文件生成路径 目前是当前cmakelist文件所在根目录
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE  ${OBJ_CPP_INCLUDE})


target_link_libraries(${PROJECT_NAME} PRIVATE
    ${DDS_LIBS}
    ${CMAKE_SOURCE_DIR}/thirdparty/CRoaring/lib/libroaring.so
    pthread
    rt
    log::log
)

target_compile_options(${PROJECT_NAME}  PRIVATE  -g )

if(BUILD_LIBRARY)
  include(GNUInstallDirs)  # ✅ 引入标准安装目录

    set(PUBLIC_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/concurrentqueue.h
    )

    install(TARGETS ${LIBRARY_OUTPUT_NAME}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_OUTPUT_NAME})  # 头文件

    message(STATUS "Library will be installed to: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif()
