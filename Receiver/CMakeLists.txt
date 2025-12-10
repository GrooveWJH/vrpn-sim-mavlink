cmake_minimum_required(VERSION 3.15)
project(vrpn_receiver_bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(SENDER_CMAKE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Sender/cmake ABSOLUTE)
list(APPEND CMAKE_MODULE_PATH ${SENDER_CMAKE_DIR})
find_package(VRPN REQUIRED)

add_executable(vrpn_receiver
    src/main.cpp
    src/TrackerClient.cpp
    src/MavlinkSender.cpp
)

target_include_directories(vrpn_receiver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mavlink
)

target_link_libraries(vrpn_receiver PRIVATE VRPN::vrpn)

install(TARGETS vrpn_receiver RUNTIME DESTINATION bin)
