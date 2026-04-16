cmake_minimum_required(VERSION 3.16)
project(DEMO  LANGUAGES  C CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_FASTDDS "Use FastDDS implementation" ON)
option(USE_TXDDS "Use TXDDS implementation" OFF)

if(USE_FASTDDS AND USE_TXDDS)
    message(FATAL_ERROR "Cannot enable both USE_FASTDDS and USE_TXDDS")
endif()

if(USE_FASTDDS)
    add_definitions(-DUSE_FASTDDS)
    message(STATUS "Using FastDDS implementation")
    set(DDS_IMPL FASTDDS)
elseif(USE_TXDDS)
    add_definitions(-DUSE_TXDDS)
    message(STATUS "Using TXDDS implementation")
    set(DDS_IMPL TXDDS)
else()
    message(FATAL_ERROR "Must enable either USE_FASTDDS or USE_TXDDS")
endif()

add_subdirectory(common/log)
add_subdirectory(core)


option(ENABLE_TESTS "Enable unit tests" OFF)
if(ENABLE_TESTS)
    add_subdirectory(tests)
endif()


option(SAMPLE "Build example" ON)
if(SAMPLE)
    add_subdirectory(sample/node_example)
    add_subdirectory(sample/data_memory_manger)
endif()
