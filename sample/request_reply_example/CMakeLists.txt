cmake_minimum_required(VERSION 3.16)

project(request_reply_example_exec  LANGUAGES  CXX)
set(LIBRARY_OUTPUT_NAME  test)



#头文件
set(OBJ_CPP_INCLUDE
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/include 
)


file(GLOB SOURCES_CC_FOR_API 
"${CMAKE_SOURCE_DIR}/common/fastdds_replyrequest_wrapper/*.cpp")
                                        
file(GLOB SOURCES_CC_FOR_XTYPE
"${CMAKE_CURRENT_SOURCE_DIR}/RequestReplyTypes/*.cxx"
"${CMAKE_CURRENT_SOURCE_DIR}/RequestReplyTypes/*.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/RequestReplyTypes/*.ipp"
)      

file(GLOB SOURCES_CC_FOR_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

message(STATUS "Collected CPP sources: ${SOURCES_CC_FOR_API}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")

# 合并所有查找到的文件列表
set(MAIN_SOURCES ${SOURCES_CC_FOR_MAIN} ${SOURCES_CC_FOR_API} ${SOURCES_CC_FOR_XTYPE})

# 添加可执行文件目标
add_executable(${PROJECT_NAME} ${MAIN_SOURCES})
#指定可执行文件生成路径 目前是当前cmakelist文件所在根目录
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE  ${OBJ_CPP_INCLUDE})


find_library(FASTRTPS_LIBRARY NAMES fastdds PATHS ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib)
find_library(FOONATHAN_MEMORY_LIBRARY NAMES foonathan_memory-0.7.3 PATHS ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib)
find_library(FASTCDR_LIBRARY NAMES fastcdr PATHS ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib)

# 检查每个库是否都找到了
if(FASTRTPS_LIBRARY AND FOONATHAN_MEMORY_LIBRARY AND FASTCDR_LIBRARY)
    list(APPEND OBJ_CPP_LIBS ${FASTRTPS_LIBRARY} ${FOONATHAN_MEMORY_LIBRARY} ${FASTCDR_LIBRARY})
    message(STATUS "Found all FAST DDS libraries")
else()
    message(FATAL_ERROR "Could not find all required FAST DDS libraries")
endif()



target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_system.a
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_thread.a
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_filesystem.a 
    ${CMAKE_SOURCE_DIR}/thirdparty/yaml-cpp/lib/libyaml-cpp.a
    ${OBJ_CPP_LIBS}
    pthread 
    log::log
    rt
)

target_compile_options(${PROJECT_NAME}  PRIVATE  -g )
