cmake_minimum_required(VERSION 3.16)

project(node_example_exec  LANGUAGES CXX)

# 查找目录下的所有源文件
# 假设您希望在查找源文件时排除 CMakeFiles 目录
file(GLOB DDS_SHM_HELLOWORLD_EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldOne/source/*.cxx"
                                                     "${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldOne/source/*.cpp"
                                                     "${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldOne/source/*.ipp"
                                                     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

file(GLOB DDS_INTERFACE_SOURCES_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../common/fastdds_wrapper/*.cpp")


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

#设置库路径
set(OBJ_CPP_LIBRARY_PATH  ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)

# 查找每个库
find_library(FASTRTPS_LIBRARY NAMES fastdds PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)
find_library(FOONATHAN_MEMORY_LIBRARY NAMES foonathan_memory-0.7.3 PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)
find_library(FASTCDR_LIBRARY NAMES fastcdr PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)

# 检查每个库是否都找到了
if(FASTRTPS_LIBRARY AND FOONATHAN_MEMORY_LIBRARY AND FASTCDR_LIBRARY)
    list(APPEND OBJ_CPP_LIBS ${FASTRTPS_LIBRARY} ${FOONATHAN_MEMORY_LIBRARY} ${FASTCDR_LIBRARY})
    message(STATUS "Found all FAST DDS libraries")
else()
    message(FATAL_ERROR "Could not find all required FAST DDS libraries")
endif()

# link_directories(${LIBRARY_PATH})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OBJ_CPP_LIBS} pthread  ${CMAKE_SOURCE_DIR}/common/log/lib/liblog.a)


# option(USE_SYSTEM_RPATH "Use system RPATH for installed libraries and executables" ON)
# if (USE_SYSTEM_RPATH)
set(CMAKE_INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
# endif()

# target_compile_options(${PROJECT_NAME}  PRIVATE -Wall -Wextra)


