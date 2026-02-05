cmake_minimum_required(VERSION 3.16)
project(DEMO  LANGUAGES  C CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_FASTDDS "Use FastDDS implementation" ON)

if(USE_FASTDDS)
    add_definitions(-DUSE_FASTDDS)
    message(STATUS "Using FastDDS implementation")
else()
    add_definitions(-DUSE_TXDDS)
    message(STATUS "Using TXDDS implementation")
endif()

add_subdirectory(common/log)
add_subdirectory(core)
add_subdirectory(tools/monitor)


option(ENABLE_TESTS "Enable unit tests" ON)
if(ENABLE_TESTS)
    add_subdirectory(tests)
endif()


option(SAMPLE "Build example" ON)
if(SAMPLE)
    add_subdirectory(sample/node_example)
    add_subdirectory(sample/request_reply_example)
    add_subdirectory(sample/shm_example)
endif()





