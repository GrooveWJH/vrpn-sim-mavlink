cmake_minimum_required(VERSION 3.15)
project(raw_vrpn_receiver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(raw_vrpn_core
    src/socket_io.cpp
    src/vrpn_wire.cpp
)

target_include_directories(raw_vrpn_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(raw_vrpn_wire_tests
    tests/test_vrpn_wire.cpp
)

target_link_libraries(raw_vrpn_wire_tests PRIVATE raw_vrpn_core)

add_executable(raw_vrpn_pose_monitor
    src/main.cpp
)

target_link_libraries(raw_vrpn_pose_monitor PRIVATE raw_vrpn_core)
target_link_libraries(raw_vrpn_pose_monitor PRIVATE m)

enable_testing()
add_test(NAME raw_vrpn_wire_tests COMMAND raw_vrpn_wire_tests)
