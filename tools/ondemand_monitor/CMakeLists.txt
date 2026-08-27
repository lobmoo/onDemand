cmake_minimum_required(VERSION 3.14)
project(ondemand_monitor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -pthread")

# FTXUI - use pre-built library from thirdparty
set(FTXUI_DIR ${CMAKE_SOURCE_DIR}/thirdparty/ftxui)

# Create imported targets for FTXUI
add_library(ftxui::screen STATIC IMPORTED)
set_target_properties(ftxui::screen PROPERTIES
  IMPORTED_LOCATION "${FTXUI_DIR}/lib/libftxui-screen.a"
  INTERFACE_INCLUDE_DIRECTORIES "${FTXUI_DIR}/include"
)

add_library(ftxui::dom STATIC IMPORTED)
set_target_properties(ftxui::dom PROPERTIES
  IMPORTED_LOCATION "${FTXUI_DIR}/lib/libftxui-dom.a"
  INTERFACE_INCLUDE_DIRECTORIES "${FTXUI_DIR}/include"
)

add_library(ftxui::component STATIC IMPORTED)
set_target_properties(ftxui::component PROPERTIES
  IMPORTED_LOCATION "${FTXUI_DIR}/lib/libftxui-component.a"
  INTERFACE_INCLUDE_DIRECTORIES "${FTXUI_DIR}/include"
)

# Find libpcap
find_library(PCAP_LIBRARY pcap)
if(NOT PCAP_LIBRARY)
  # Try common locations (runtime library without -dev package)
  set(PCAP_LIBRARY "/usr/lib/x86_64-linux-gnu/libpcap.so.0.8")
  if(NOT EXISTS "${PCAP_LIBRARY}")
    message(FATAL_ERROR "libpcap not found. Install with: sudo apt install libpcap-dev")
  endif()
endif()

# Find fastcdr
set(FASTCDR_LIB_DIR ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/lib)
set(FASTCDR_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/fast_dds/include)

find_library(FASTCDR_LIBRARY fastcdr
  PATHS ${FASTCDR_LIB_DIR}
  NO_DEFAULT_PATH
)

# Include directories
set(MONITOR_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${FASTCDR_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/common
  ${CMAKE_SOURCE_DIR}/common/log
  ${CMAKE_SOURCE_DIR}/core/ondemand  # for concurrentqueue.h
)

# Add pcap include if using local header
set(PCAP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${MONITOR_INCLUDE_DIRS} ${PCAP_INCLUDE_DIRS})

# Source files
set(MONITOR_SOURCES
  main.cc
  pcap_worker.cc
  rtps_parser.cc
  metrics_engine.cc
  match_analyzer.cc
  monitor_ui.cc
)

# Executable
add_executable(ondemand_monitor ${MONITOR_SOURCES})

# Link libraries
target_link_libraries(ondemand_monitor
  ftxui::component
  ftxui::dom
  ftxui::screen
  ${PCAP_LIBRARY}
  ${FASTCDR_LIBRARY}
  pthread
)

# Unit tests (engine statistics logic; no pcap/FTXUI dependencies)
enable_testing()
foreach(monitor_test engine_repro_test stats_logic_test)
  add_executable(${monitor_test}
    tests/${monitor_test}.cc
    metrics_engine.cc
    rtps_parser.cc
  )
  add_test(NAME ${monitor_test} COMMAND ${monitor_test})
endforeach()

# Installation
install(TARGETS ondemand_monitor DESTINATION bin)
