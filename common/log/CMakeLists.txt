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

project(log  LANGUAGES  CXX)

set(LIBRARY_OUTPUT_NAME  ${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 17) 
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER g++)

# 指定安装路径的变量，可以在 CMake 命令行中覆盖
set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib")
set(INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")


#头文件
set(OBJ_CPP_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/spdlog/include
)

set(SOURCES_CC_FOR_LOG
    "${CMAKE_CURRENT_SOURCE_DIR}/logger.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/logger_impl.cc"
)

message(STATUS "Collected CXX sources: ${SOURCES_CC_FOR_LOG}")
message(STATUS "Collected CXX include directories: ${OBJ_CPP_INCLUDE}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")


if(BUILD_TESTING)
endif()

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
# 创建库，类型取决于 BUILD_SHARED_LIBS


add_library(${LIBRARY_OUTPUT_NAME}   ${SOURCES_CC_FOR_LOG})
target_include_directories(${LIBRARY_OUTPUT_NAME} PRIVATE   ${OBJ_CPP_INCLUDE})
set_target_properties(${LIBRARY_OUTPUT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
    
# 头文件拷贝到 include 目录
file(GLOB LOG_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/logger.h")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)
foreach(header ${LOG_HEADERS})
    configure_file(${header} ${CMAKE_CURRENT_SOURCE_DIR}/include/ COPYONLY)
endforeach()

    
install(TARGETS ${LIBRARY_OUTPUT_NAME}
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
        LIBRARY DESTINATION ${INSTALL_LIB_DIR})  # 动态库和可执行文件也可以放在 bin
# 安装头文件到 include 目录
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/
        DESTINATION ${INSTALL_INCLUDE_DIR}
        FILES_MATCHING PATTERN "*.h")
