cmake_minimum_required(VERSION 3.16)
project(test_out  LANGUAGES  CXX)



file(GLOB_RECURSE API_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../test*.cpp"  
    "${CMAKE_CURRENT_SOURCE_DIR}/../*_test.cc"   
)

#头文件
set(API_TEST_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/nlohmann/include
)

# 添加可执行文件目标
add_executable(${PROJECT_NAME} ${API_TEST_SOURCES})
# #指定可执行文件生成路径 目前是当前cmakelist文件所在根目录
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE  ${API_TEST_INCLUDE})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    log::log
    gtest
    gtest_main
    pthread 
    rt
)

target_compile_options(${PROJECT_NAME}  PRIVATE  -O2)
