cmake_minimum_required(VERSION 3.16)

option(COMPILE_SAMPLE "Build example" ON)

project(DEMO  LANGUAGES  CXX)

add_subdirectory(common/log)
add_subdirectory(core)
add_subdirectory(tools/monitor)


# if(TEST)
# {
#     enable_testing()
#     add_subdirectory(tests)
# }

if(COMPILE_SAMPLE)
    add_subdirectory(sample/node_example)
    add_subdirectory(sample/request_reply_example)
endif()





