cmake_minimum_required(VERSION 3.16)

project(NGVS_example_exec  LANGUAGES CXX)

set(LIBRARY_OUTPUT_NAME  ${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 17) 
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER g++)

# 指定安装路径的变量，可以在 CMake 命令行中覆盖
set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib")
set(INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")


file(GLOB DDS_INTERFACE_SOURCES_CPP "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")


message(STATUS "Collected CPP sources: ${DDS_INTERFACE_SOURCES_CPP}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")


# 添加可执行文件目标
add_executable(${PROJECT_NAME}  ${DDS_INTERFACE_SOURCES_CPP})
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

#头文件
set(OBJ_CPP_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/boost/include
    ${CMAKE_CURRENT_SOURCE_DIR}/
)
target_include_directories(${PROJECT_NAME} PRIVATE  ${OBJ_CPP_INCLUDE})


# link_directories(${LIBRARY_PATH})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OBJ_CPP_LIBS} pthread  ${CMAKE_SOURCE_DIR}/common/log/lib/liblog.a)


install(TARGETS ${LIBRARY_OUTPUT_NAME}
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
        LIBRARY DESTINATION ${INSTALL_LIB_DIR})  # 动态库和可执行文件也可以放在 bin
# 安装头文件到 include 目录
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/
        DESTINATION ${INSTALL_INCLUDE_DIR}
        FILES_MATCHING PATTERN "*.h")


target_compile_options(${PROJECT_NAME}  PRIVATE -g)


