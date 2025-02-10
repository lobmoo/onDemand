# Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.16)

project(test_exec  LANGUAGES  CXX)

set(LIBRARY_OUTPUT_NAME  test)
set(CMAKE_CXX_STANDARD 17) 
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER g++)

# 指定安装路径的变量，可以在 CMake 命令行中覆盖
set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib")
set(INSTALL_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")



#头文件
set(OBJ_CPP_INCLUDE
    ${CMAKE_SOURCE_DIR}/src/spdlog
    ${CMAKE_SOURCE_DIR}/thirdparty/BS_thread_pool/include
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/include
    ${CMAKE_SOURCE_DIR}/thirdparty/spdlog/include
)

set(SOURCES_CC_FOR_LOG
    "${CMAKE_CURRENT_SOURCE_DIR}/log/logger.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/log/logger_impl.cc"
)

set(SOURCES_CC_FOR_SPD_LOG
    "${CMAKE_CURRENT_SOURCE_DIR}/spdlog/logger.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/spdlog/logger_impl.cc"
)

# 查找目录下的所有源文件
# 假设您希望在查找源文件时排除 CMakeFiles 目录
file(GLOB SOURCES_CC_FOR_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
#list(FILTER DDS_SHM_HELLOWORLD_EXAMPLE_SOURCES_CPP EXCLUDE REGEX "CMakeFiles/.*")

message(STATUS "Collected CXX sources: ${SOURCES_CC_FOR_LOG}")
message(STATUS "Collected CPP sources: ${SOURCES_CC_FOR_MAIN}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")

# 合并所有查找到的文件列表
set(ALL_SOURCES ${SOURCES_CC_FOR_SPD_LOG} ${SOURCES_CC_FOR_MAIN})
# 复制源文件列表用于不同的目标
set(MAIN_SOURCES ${ALL_SOURCES})



# 添加测试选项
option(BUILD_TESTING "Build the testing targets" ON)

if(BUILD_TESTING)
    # 添加测试源文件
#     set(SOURCES_CC_FOR_TEST
#         "${CMAKE_CURRENT_SOURCE_DIR}/log/logger_test.cc"
#     )
    set(SOURCES_SPDLOG
        "${CMAKE_CURRENT_SOURCE_DIR}/spdlog/spdlog_test.cc"
    )    

    # 添加测试可执行文件
#     add_executable(logger_test ${SOURCES_CC_FOR_LOG} ${SOURCES_CC_FOR_TEST})
    add_executable(spdlog_test ${SOURCES_SPDLOG} ${SOURCES_CC_FOR_SPD_LOG})
#     set_target_properties(logger_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test") 
    set_target_properties(spdlog_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test") 
#     target_include_directories(logger_test PRIVATE   ${OBJ_CPP_INCLUDE})
    target_include_directories(spdlog_test PRIVATE   ${OBJ_CPP_INCLUDE})
    # 链接测试所需的库
#     target_link_libraries(logger_test
#         PRIVATE
#         gtest
#         gtest_main
#         ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_log.a 
#         ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_log_setup.a
#         ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_system.a
#         ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_thread.a
#         ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_filesystem.a
#         ${CMAKE_SOURCE_DIR}/thirdparty/spdlog/lib/libspdlog.a
#         pthread 
#         rt
#     )
    target_link_libraries(spdlog_test
    PRIVATE
    gtest
    gtest_main
    ${CMAKE_SOURCE_DIR}/thirdparty/spdlog/lib/libspdlog.a
    pthread 
    rt
)
endif()

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
# 创建库，类型取决于 BUILD_SHARED_LIBS

if(BUILD_LIBRARY)
add_library(${LIBRARY_OUTPUT_NAME}   ${SOURCES_CC_FOR_LOG})
target_include_directories(${LIBRARY_OUTPUT_NAME} PRIVATE   ${OBJ_CPP_INCLUDE})
endif()

# 添加可执行文件目标
add_executable(${PROJECT_NAME} ${MAIN_SOURCES})
#指定可执行文件生成路径 目前是当前cmakelist文件所在根目录
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

target_include_directories(${PROJECT_NAME} PRIVATE  ${OBJ_CPP_INCLUDE})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_log.a 
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_log_setup.a
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_system.a
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_thread.a
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_filesystem.a
    ${CMAKE_SOURCE_DIR}/thirdparty/spdlog/lib/libspdlog.a
    pthread 
    rt
)

target_compile_options(${PROJECT_NAME}  PRIVATE  -g )

if(BUILD_LIBRARY)
# 安装静态库到 lib 目录
install(TARGETS ${LIBRARY_OUTPUT_NAME}
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
        LIBRARY DESTINATION ${INSTALL_LIB_DIR})  # 动态库和可执行文件也可以放在 bin
endif()

# 安装可执行文件到 bin 目录
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${INSTALL_BIN_DIR})

# 安装头文件到 include 目录
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/
        DESTINATION ${INSTALL_INCLUDE_DIR}
        FILES_MATCHING PATTERN "*.h")
