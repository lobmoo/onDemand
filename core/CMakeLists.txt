cmake_minimum_required(VERSION 3.16)

project(demo_exec  LANGUAGES  CXX)
set(LIBRARY_OUTPUT_NAME  demo)

#头文件
set(OBJ_CPP_INCLUDE
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/thirdparty/tinyxml/include
    ${CMAKE_SOURCE_DIR}/thirdparty/BS_thread_pool/include
)


# set(SOURCES_CC_FOR_NET_WORK
#     "${CMAKE_CURRENT_SOURCE_DIR}/net_interface.cc"
# )
                              
file(GLOB SOURCES_CC_FOR_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")


# 合并所有查找到的文件列表
set(MAIN_SOURCES ${SOURCES_CC_FOR_MAIN} )


option(BUILD_LIBRARY "Build library" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)


if(BUILD_LIBRARY)
    add_library(${LIBRARY_OUTPUT_NAME} ${MAIN_SOURCES})  # 添加源文件
    target_include_directories(${LIBRARY_OUTPUT_NAME} PRIVATE ${OBJ_CPP_INCLUDE})
    
    # 库也需要链接依赖
    target_link_libraries(${LIBRARY_OUTPUT_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_system.a
        ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_thread.a
        ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_filesystem.a 
        ${CMAKE_SOURCE_DIR}/thirdparty/yaml-cpp/lib/libyaml-cpp.a
        ${CMAKE_SOURCE_DIR}/thirdparty/tinyxml/lib/libtinyxml2.a 
        pthread 
        rt
        log::log 
    )
    
    target_compile_options(${LIBRARY_OUTPUT_NAME} PRIVATE -fPIC -O3)
endif()


# 添加可执行文件目标
add_executable(${PROJECT_NAME} ${MAIN_SOURCES})
#指定可执行文件生成路径 目前是当前cmakelist文件所在根目录
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE  ${OBJ_CPP_INCLUDE})


target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_system.a
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_thread.a
    ${CMAKE_SOURCE_DIR}/thirdparty/boost/lib/libboost_filesystem.a 
    ${CMAKE_SOURCE_DIR}/thirdparty/yaml-cpp/lib/libyaml-cpp.a
    ${CMAKE_SOURCE_DIR}/thirdparty/tinyxml/lib/libtinyxml2.a 
    pthread 
    rt
    log::log 
)

target_compile_options(${PROJECT_NAME}  PRIVATE  -g )

if(BUILD_LIBRARY)
  include(GNUInstallDirs)  # ✅ 引入标准安装目录
    
    set(PUBLIC_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/concurrentqueue.h
    )
    
    
    install(TARGETS ${LIBRARY_OUTPUT_NAME}
            EXPORT ${LIBRARY_OUTPUT_NAME}Targets  # 支持find_package
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_OUTPUT_NAME})  # 头文件
    
    
    install(EXPORT ${LIBRARY_OUTPUT_NAME}Targets
            FILE ${LIBRARY_OUTPUT_NAME}Targets.cmake
            NAMESPACE ${LIBRARY_OUTPUT_NAME}::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${LIBRARY_OUTPUT_NAME})
            
    message(STATUS "Library will be installed to: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif()