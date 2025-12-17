cmake_minimum_required(VERSION 3.16)

project(node_example_exec  LANGUAGES CXX)

# 查找目录下的所有源文件
# 假设您希望在查找源文件时排除 CMakeFiles 目录
file(GLOB DDS_SHM_HELLOWORLD_EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldOne/source/*.cxx"
                                                     "${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldOne/source/*.cpp"
                                                     "${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldOne/source/*.ipp"
                                                     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

file(GLOB DDS_INTERFACE_SOURCES_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../common/fastdds_wrapper/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../common/fastdds_wrapper/*.cc")


message(STATUS "Collected CXX sources: ${DDS_SHM_HELLOWORLD_EXAMPLE_SOURCES}")
message(STATUS "Collected CPP sources: ${DDS_INTERFACE_SOURCES_CPP}")

# 添加可执行文件目标
add_executable(${PROJECT_NAME} ${DDS_SHM_HELLOWORLD_EXAMPLE_SOURCES}  ${DDS_INTERFACE_SOURCES_CPP})
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

#头文件
set(OBJ_CPP_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldOne/source
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/include
)
list(APPEND OBJ_CPP_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/include)
target_include_directories(${PROJECT_NAME} PRIVATE  ${OBJ_CPP_INCLUDE})



target_link_libraries(${PROJECT_NAME} PRIVATE  
    ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfastdds.so
    ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfastcdr.so
    ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib/libfoonathan_memory-0.7.3.so
    log::log  
    pthread
)

# option(USE_SYSTEM_RPATH "Use system RPATH for installed libraries and executables" ON)
# if (USE_SYSTEM_RPATH)
set(CMAKE_INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
# endif()

target_compile_options(${PROJECT_NAME}  PRIVATE -g)


