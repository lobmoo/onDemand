# Copyright 2021 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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

if(NOT CMAKE_VERSION VERSION_LESS 3.0)
    cmake_policy(SET CMP0048 NEW)
endif()

project(monitor_exec)


#Check C++11
include(CheckCXXCompilerFlag)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG OR
        CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    check_cxx_compiler_flag(-std=c++11 SUPPORTS_CXX11)
    if(NOT SUPPORTS_CXX11)
        message(FATAL_ERROR "Compiler doesn't support C++11")
    endif()
endif()

file(GLOB HELLOWORLD_EXAMPLE_SOURCES_CXX "*.cxx")
file(GLOB HELLOWORLD_EXAMPLE_SOURCES_CPP "*.cpp")

add_executable(${PROJECT_NAME} ${HELLOWORLD_EXAMPLE_SOURCES_CXX} ${HELLOWORLD_EXAMPLE_SOURCES_CPP})
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<STREQUAL:"${CMAKE_BUILD_TYPE}","Debug">>:__DEBUG>
    $<$<BOOL:${INTERNAL_DEBUG}>:__INTERNALDEBUG> # Internal debug activated.
)

set(OBJ_CPP_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/nlohmann/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/ftxui/include/
)
target_include_directories(${PROJECT_NAME} PRIVATE  ${OBJ_CPP_INCLUDE})


# 查找每个库
find_library(FASTRTPS_LIBRARY NAMES fastdds PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)
find_library(FOONATHAN_MEMORY_LIBRARY NAMES foonathan_memory-0.7.3 PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)
find_library(FASTCDR_LIBRARY NAMES fastcdr PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)
find_library(FASTDDS_STATISTICS_BACKEND NAMES fastdds_statistics_backend PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)

# 检查每个库是否都找到了
if(FASTRTPS_LIBRARY AND FOONATHAN_MEMORY_LIBRARY AND FASTCDR_LIBRARY AND FASTDDS_STATISTICS_BACKEND)
    list(APPEND OBJ_CPP_LIBS ${FASTRTPS_LIBRARY} ${FOONATHAN_MEMORY_LIBRARY} ${FASTCDR_LIBRARY})
    message(STATUS "Found all FAST DDS libraries")
else()
    message(FATAL_ERROR "Could not find all required FAST DDS libraries")
endif()


target_link_libraries(${PROJECT_NAME} ${OBJ_CPP_LIBS}   
${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/ftxui/lib/libftxui-component.a 
${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/ftxui/lib/libftxui-dom.a 
${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/ftxui/lib/libftxui-screen.a
${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib/libfastdds_statistics_backend.so    
${CMAKE_SOURCE_DIR}/common/log/lib/liblog.a                                                        
pthread)

target_compile_options(${PROJECT_NAME}  PRIVATE -g -std=c++17)