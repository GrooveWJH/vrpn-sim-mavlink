cmake_minimum_required(VERSION 3.15)
project(fake_vrpn_uav_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(VRPN REQUIRED)

add_executable(fake_vrpn_uav_server src/fake_vrpn_uav_server.cpp)
target_link_libraries(fake_vrpn_uav_server PRIVATE VRPN::vrpn)

install(TARGETS fake_vrpn_uav_server RUNTIME DESTINATION bin)
