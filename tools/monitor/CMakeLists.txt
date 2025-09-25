cmake_minimum_required(VERSION 3.16)
project(monitor_exec)


file(GLOB HELLOWORLD_EXAMPLE_SOURCES_CXX "*.cxx")
file(GLOB HELLOWORLD_EXAMPLE_SOURCES_CPP "*.cpp")

add_executable(${PROJECT_NAME} ${HELLOWORLD_EXAMPLE_SOURCES_CXX} ${HELLOWORLD_EXAMPLE_SOURCES_CPP})
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<STREQUAL:"${CMAKE_BUILD_TYPE}","Debug">>:__DEBUG>
    $<$<BOOL:${INTERNAL_DEBUG}>:__INTERNALDEBUG> # Internal debug activated.
)

set(OBJ_CPP_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/nlohmann/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/ftxui/include/
)
target_include_directories(${PROJECT_NAME} PRIVATE  ${OBJ_CPP_INCLUDE})


# 查找每个库
find_library(FASTRTPS_LIBRARY NAMES fastdds PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)
find_library(FOONATHAN_MEMORY_LIBRARY NAMES foonathan_memory-0.7.3 PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)
find_library(FASTCDR_LIBRARY NAMES fastcdr PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)
find_library(FASTDDS_STATISTICS_BACKEND NAMES fastdds_statistics_backend PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib)

# 检查每个库是否都找到了
if(FASTRTPS_LIBRARY AND FOONATHAN_MEMORY_LIBRARY AND FASTCDR_LIBRARY AND FASTDDS_STATISTICS_BACKEND)
    list(APPEND OBJ_CPP_LIBS ${FASTRTPS_LIBRARY} ${FOONATHAN_MEMORY_LIBRARY} ${FASTCDR_LIBRARY})
    message(STATUS "Found all FAST DDS libraries")
else()
    message(FATAL_ERROR "Could not find all required FAST DDS libraries")
endif()


target_link_libraries(${PROJECT_NAME} ${OBJ_CPP_LIBS}   
${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/ftxui/lib/libftxui-component.a 
${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/ftxui/lib/libftxui-dom.a 
${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/ftxui/lib/libftxui-screen.a
${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/fast_dds/lib/libfastdds_statistics_backend.so    
log::log                                                     
pthread)

target_compile_options(${PROJECT_NAME}  PRIVATE -g -std=c++17)