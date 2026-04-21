# Minimal VRPN finder for standalone builds on Linux/macOS.

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_VRPN QUIET vrpn)
endif()

find_path(VRPN_INCLUDE_DIR
          NAMES vrpn_Connection.h
          HINTS ${PC_VRPN_INCLUDE_DIRS}
          PATHS
              /opt/ros/jazzy
              /opt/homebrew
              /usr
              /usr/local
              /opt/local
          PATH_SUFFIXES include)

find_library(VRPN_vrpn_LIBRARY
             NAMES vrpn
             HINTS ${PC_VRPN_LIBRARY_DIRS}
             PATHS
                 /opt/ros/jazzy/lib
                 /opt/homebrew/lib
                 /usr/lib
                 /usr/lib/x86_64-linux-gnu
                 /usr/lib/aarch64-linux-gnu
                 /usr/local/lib
                 /opt/local/lib)

find_library(VRPN_quat_LIBRARY
             NAMES quat
             HINTS ${PC_VRPN_LIBRARY_DIRS}
             PATHS
                 /opt/ros/jazzy/lib
                 /opt/homebrew/lib
                 /usr/lib
                 /usr/lib/x86_64-linux-gnu
                 /usr/lib/aarch64-linux-gnu
                 /usr/local/lib
                 /opt/local/lib)

set(VRPN_LIBRARIES ${VRPN_vrpn_LIBRARY})
if(VRPN_quat_LIBRARY)
    list(APPEND VRPN_LIBRARIES ${VRPN_quat_LIBRARY})
endif()

set(VRPN_INCLUDE_DIRS ${VRPN_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VRPN REQUIRED_VARS VRPN_INCLUDE_DIR VRPN_vrpn_LIBRARY)

if(VRPN_FOUND AND NOT TARGET VRPN::vrpn)
    add_library(VRPN::vrpn INTERFACE IMPORTED)
    target_include_directories(VRPN::vrpn INTERFACE ${VRPN_INCLUDE_DIRS})
    target_link_libraries(VRPN::vrpn INTERFACE ${VRPN_LIBRARIES})
endif()
