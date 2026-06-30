cmake_minimum_required(VERSION 3.16)
project(modbuscpp LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings as errors
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Library option: static or shared
option(MODBUSCPP_SHARED "Build modbuscpp as shared library" OFF)

# Library target
add_library(modbuscpp
  src/crc.cpp
  src/pdu.cpp
  src/tcp_link.cpp
  src/rtu_link.cpp
  src/client.cpp
  src/convert.cpp
)

target_include_directories(modbuscpp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if(WIN32)
  target_link_libraries(modbuscpp PUBLIC ws2_32)
endif()

if(MODBUSCPP_SHARED)
  set_target_properties(modbuscpp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )
endif()

# Tests
enable_testing()
include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)

add_executable(modbuscpp_tests
  tests/test_crc.cpp
  tests/test_pdu.cpp
  tests/test_tcp_link.cpp
  tests/test_rtu_link.cpp
  tests/test_client.cpp
  tests/test_convert.cpp
)

target_link_libraries(modbuscpp_tests PRIVATE modbuscpp doctest)
target_include_directories(modbuscpp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_test(NAME modbuscpp_tests COMMAND modbuscpp_tests)

# CLI tool
add_executable(modbus_tool
  tools/modbus_tool.cpp
)
target_link_libraries(modbus_tool PRIVATE modbuscpp)
target_include_directories(modbus_tool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(WIN32)
  target_link_libraries(modbus_tool PRIVATE ws2_32)
endif()
