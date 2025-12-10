# Minimal VRPN finder for systems where the library was installed via Homebrew or source builds.

find_path(VRPN_INCLUDE_DIR
          NAMES vrpn_Connection.h
          PATHS
              /opt/homebrew
              /usr/local
              /opt/local
          PATH_SUFFIXES include)

find_library(VRPN_vrpn_LIBRARY NAMES vrpn
             PATHS /opt/homebrew/lib /usr/local/lib /opt/local/lib)
find_library(VRPN_quat_LIBRARY NAMES quat
             PATHS /opt/homebrew/lib /usr/local/lib /opt/local/lib)

set(VRPN_LIBRARIES ${VRPN_vrpn_LIBRARY} ${VRPN_quat_LIBRARY})
set(VRPN_INCLUDE_DIRS ${VRPN_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VRPN REQUIRED_VARS VRPN_INCLUDE_DIR VRPN_vrpn_LIBRARY VRPN_quat_LIBRARY)

if(VRPN_FOUND AND NOT TARGET VRPN::vrpn)
    add_library(VRPN::vrpn INTERFACE IMPORTED)
    target_include_directories(VRPN::vrpn INTERFACE ${VRPN_INCLUDE_DIRS})
    target_link_libraries(VRPN::vrpn INTERFACE ${VRPN_LIBRARIES})
endif()
