cmake_minimum_required(VERSION 3.16)

option(COMPILE_SAMPLE "Build example" ON)

project(DEMO  LANGUAGES  CXX)

add_subdirectory(common/log)
add_subdirectory(core)
add_dependencies(test_exec log)

 add_subdirectory(tools/monitor)

if(COMPILE_SAMPLE)
    add_subdirectory(sample/node_example)
    add_subdirectory(sample/node_test)
    add_subdirectory(sample/request_reply_example)
    add_subdirectory(sample/request_reply_test)
    add_subdirectory(sample/NGVS)
    add_subdirectory(sample/security)
endif()





