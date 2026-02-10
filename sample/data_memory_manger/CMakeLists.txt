cmake_minimum_required(VERSION 3.16)

project(data_example_exec  LANGUAGES CXX)

# 查找目录下的所有源文件

file(GLOB MAIN_CC "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

message(STATUS "Collected CPP sources: ${SHM_TRANSPORT_CC}")

# 添加可执行文件目标
add_executable(${PROJECT_NAME} ${SHM_TRANSPORT_CC} ${MAIN_CC})

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sample")

#头文件
set(OBJ_CPP_INCLUDE
    
)

target_include_directories(${PROJECT_NAME} PRIVATE  ${OBJ_CPP_INCLUDE})

target_link_libraries(${PROJECT_NAME} PRIVATE  
    log::log  
    pthread
    rt
)

target_compile_options(${PROJECT_NAME}  PRIVATE -g)


