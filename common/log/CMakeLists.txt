
cmake_minimum_required(VERSION 3.16)

project(log VERSION 1.0.0  LANGUAGES  CXX)

set(LIBRARY_OUTPUT_NAME  ${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 17) 


#头文件
set(OBJ_CPP_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/nlohmann/include
)

set(SOURCES_CC_FOR_LOG
    "${CMAKE_CURRENT_SOURCE_DIR}/logger.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/logger_impl.cc"
)


option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
add_library(${LIBRARY_OUTPUT_NAME}   ${SOURCES_CC_FOR_LOG})
target_include_directories(${LIBRARY_OUTPUT_NAME} PRIVATE   ${OBJ_CPP_INCLUDE})

target_link_libraries(${PROJECT_NAME} PRIVATE pthread )

target_compile_options(${PROJECT_NAME}  PRIVATE  -O3 -fPIC)

# 导出目标
add_library(log::log ALIAS log)



# 安装配置（可选）
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 只在作为主项目时才安装
    include(GNUInstallDirs)
    
    set(LOG_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/logger.h"
   )

    install(TARGETS log
            EXPORT logTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    
    install(FILES ${LOG_HEADERS}
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/log
    )
    
    install(EXPORT logTargets
            FILE logTargets.cmake
            NAMESPACE log::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/log
    )
endif()