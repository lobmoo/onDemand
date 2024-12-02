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

# 指定安装路径的变量，可以在 CMake 命令行中覆盖
set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib")
set(INSTALL_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")

# 查找目录下的所有源文件
# 假设您希望在查找源文件时排除 CMakeFiles 目录
file(GLOB SOURCES_CC_FOR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")
#list(FILTER DDS_SHM_HELLOWORLD_EXAMPLE_SOURCES_CXX EXCLUDE REGEX "CMakeFiles/.*")

file(GLOB SOURCES_CC_FOR_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
#list(FILTER DDS_SHM_HELLOWORLD_EXAMPLE_SOURCES_CPP EXCLUDE REGEX "CMakeFiles/.*")

message(STATUS "Collected CXX sources: ${SOURCES_CC_FOR_SRC}")
message(STATUS "Collected CPP sources: ${SOURCES_CC_FOR_MAIN}")

# 合并所有查找到的文件列表
set(ALL_SOURCES ${SOURCES_CC_FOR_SRC} ${SOURCES_CC_FOR_MAIN})
# 复制源文件列表用于不同的目标
set(MAIN_SOURCES ${ALL_SOURCES})


option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
# 创建库，类型取决于 BUILD_SHARED_LIBS
add_library(${LIBRARY_OUTPUT_NAME}   ${SOURCES_CC_FOR_SRC})

# 添加可执行文件目标
add_executable(${PROJECT_NAME} ${MAIN_SOURCES})
#指定可执行文件生成路径 目前是当前cmakelist文件所在根目录
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")


#头文件
set(OBJ_CPP_INCLUDE "")
set(OBJ_CPP_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/BS_thread_pool/include
)


#list(APPEND OBJ_CPP_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glog/include)


target_include_directories(${PROJECT_NAME} PRIVATE  ${OBJ_CPP_INCLUDE})


# # FSAT DDS
# find_library(FASTRTPS_LIBRARY NAMES fastrtps PATHS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/fast_dds/lib)
# find_library(FOONATHAN_MEMORY_LIBRARY NAMES foonathan_memory-0.7.3 PATHS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/fast_dds/lib)
# find_library(FASTCDR_LIBRARY NAMES fastcdr PATHS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/fast_dds/lib)
# # 检查每个库是否都找到了
# if(FASTRTPS_LIBRARY AND FOONATHAN_MEMORY_LIBRARY AND FASTCDR_LIBRARY)
#     list(APPEND OBJ_CPP_LIBS ${FASTRTPS_LIBRARY} ${FOONATHAN_MEMORY_LIBRARY} ${FASTCDR_LIBRARY})
#     message(STATUS "Found all FAST DDS libraries")
# else()
#     message(FATAL_ERROR "Could not find all required FAST DDS libraries")
# endif()



target_link_libraries(${PROJECT_NAME} PRIVATE 
    pthread 
    rt
    #${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/tinyxml2/lib/libtinyxml2.a    
)

target_compile_options(${PROJECT_NAME}  PRIVATE -Wall -Wextra -g  -std=c++17)


# 安装静态库到 lib 目录
install(TARGETS ${LIBRARY_OUTPUT_NAME}
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
        LIBRARY DESTINATION ${INSTALL_LIB_DIR})  # 动态库和可执行文件也可以放在 bin

# 安装可执行文件到 bin 目录
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${INSTALL_BIN_DIR})

# 安装头文件到 include 目录
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
        DESTINATION ${INSTALL_INCLUDE_DIR}
        FILES_MATCHING PATTERN "*.h")
