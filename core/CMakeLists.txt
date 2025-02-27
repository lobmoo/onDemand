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
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/thirdparty/BS_thread_pool/include
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/include
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann/include
    ${CMAKE_SOURCE_DIR}/thirdparty/yaml-cpp/include
)


# set(SOURCES_CC_FOR_NET_WORK
#     "${CMAKE_CURRENT_SOURCE_DIR}/net_interface.cc"
# )


file(GLOB SOURCES_CC_FOR_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

message(STATUS "Collected CPP sources: ${SOURCES_CC_FOR_MAIN}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")

# 合并所有查找到的文件列表
set(MAIN_SOURCES ${SOURCES_CC_FOR_MAIN})


option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

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
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_system.a
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_thread.a
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_filesystem.a
    ${CMAKE_SOURCE_DIR}/thirdparty/yaml-cpp/lib/libyaml-cpp.a
    ${CMAKE_SOURCE_DIR}/common/log/lib/liblog.a
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
