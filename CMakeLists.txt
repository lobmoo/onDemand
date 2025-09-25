cmake_minimum_required(VERSION 3.16)

project(DEMO  LANGUAGES  C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_subdirectory(common/log)
add_subdirectory(core)
add_subdirectory(tools/monitor)


# if(TEST)
# {
#     enable_testing()
#     add_subdirectory(tests)
# }

option(COMPILE_SAMPLE "Build example" ON)

if(COMPILE_SAMPLE)
    add_subdirectory(sample/node_example)
    add_subdirectory(sample/request_reply_example)
endif()





