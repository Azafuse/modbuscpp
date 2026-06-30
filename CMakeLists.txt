set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
cmake_minimum_required(VERSION 3.16)
project(modbuscpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

option(MODBUSCPP_SHARED "Build modbuscpp as shared library" OFF)

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
  set_target_properties(modbuscpp PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# Tests
enable_testing()
include(FetchContent)
FetchContent_Declare(doctest GIT_REPOSITORY https://github.com/doctest/doctest.git GIT_TAG v2.4.11)
FetchContent_MakeAvailable(doctest)

add_executable(modbuscpp_tests
  tests/test_crc.cpp
  tests/test_pdu.cpp
  tests/test_tcp_link.cpp
  tests/test_client.cpp
  tests/test_convert.cpp
  tests/test_rtu_link.cpp
)
target_link_libraries(modbuscpp_tests PRIVATE modbuscpp doctest)
target_include_directories(modbuscpp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(WIN32)
  target_link_libraries(modbuscpp_tests PRIVATE ws2_32)
endif()
add_test(NAME modbuscpp_tests COMMAND modbuscpp_tests)

add_executable(modbus_tool tools/modbus_tool.cpp)
target_link_libraries(modbus_tool PRIVATE modbuscpp)
target_include_directories(modbus_tool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(WIN32)
  target_link_libraries(modbus_tool PRIVATE ws2_32)
endif()

add_executable(modbus_bench tools/modbus_bench.cpp)
target_link_libraries(modbus_bench PRIVATE modbuscpp)
target_include_directories(modbus_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(WIN32)
  target_link_libraries(modbus_bench PRIVATE ws2_32)
endif()

set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
add_executable(modbus_gui tools/modbus_gui.cpp)
target_link_libraries(modbus_gui PRIVATE modbuscpp Qt6::Widgets)
target_include_directories(modbus_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(WIN32)
  target_link_libraries(modbus_gui PRIVATE ws2_32)
endif()