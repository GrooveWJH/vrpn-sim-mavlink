# Minimal VRPN finder for systems where the library was installed via Homebrew, Linuxbrew, or source builds.

set(_vrpn_prefix_candidates
    $ENV{VRPN_ROOT}
    $ENV{HOMEBREW_PREFIX}
    $ENV{LINUXBREW_HOME}
    /opt/homebrew
    /usr/local
    /opt/local
    /home/linuxbrew/.linuxbrew)
list(REMOVE_DUPLICATES _vrpn_prefix_candidates)
list(FILTER _vrpn_prefix_candidates EXCLUDE REGEX "^$")

find_path(VRPN_INCLUDE_DIR
          NAMES vrpn_Connection.h
          PATHS ${_vrpn_prefix_candidates}
          PATH_SUFFIXES include)

find_library(VRPN_vrpn_LIBRARY NAMES vrpn
             PATHS ${_vrpn_prefix_candidates}
             PATH_SUFFIXES lib lib64)
find_library(VRPN_quat_LIBRARY NAMES quat
             PATHS ${_vrpn_prefix_candidates}
             PATH_SUFFIXES lib lib64)

set(VRPN_LIBRARIES ${VRPN_vrpn_LIBRARY} ${VRPN_quat_LIBRARY})
set(VRPN_INCLUDE_DIRS ${VRPN_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VRPN REQUIRED_VARS VRPN_INCLUDE_DIR VRPN_vrpn_LIBRARY VRPN_quat_LIBRARY)

if(VRPN_FOUND AND NOT TARGET VRPN::vrpn)
    add_library(VRPN::vrpn INTERFACE IMPORTED)
    target_include_directories(VRPN::vrpn INTERFACE ${VRPN_INCLUDE_DIRS})
    target_link_libraries(VRPN::vrpn INTERFACE ${VRPN_LIBRARIES})
endif()
