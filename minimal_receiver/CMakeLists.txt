cmake_minimum_required(VERSION 3.15)
project(minimal_vrpn_receiver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(VRPN REQUIRED)

add_executable(vrpn_pose_monitor
    src/main.cpp
    src/TrackerClient.cpp
)

target_include_directories(vrpn_pose_monitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vrpn_pose_monitor PRIVATE VRPN::vrpn)

install(TARGETS vrpn_pose_monitor RUNTIME DESTINATION bin)
