# Already in cache, be silent
if (Earthworm_INCLUDE_DIR AND Earthworm_MT_LIBRARY AND Earthworm_UTIL_LIBRARY)
    set(Earthworm_FIND_QUIETLY TRUE)
endif()

# Find the include directory
find_path(Earthworm_INCLUDE_DIR
          NAMES earthworm.h
          HINTS $ENV{EW_ROOT}/include
                $ENV{EW_HOME}/include
                /opt/earthworm/earthworm_7.10/include
                /opt/earthworm_7.10/include
                /usr/local/include/)
# Find the library components
find_library(Earthworm_MT_LIBRARY
             NAMES ew_mt
             PATHS $ENV{EW_ROOT}/lib
                   $ENV{EW_ROOT}/lib64
                   $ENV{EW_HOME}/lib
                   $ENV{EW_HOME}/lib64
                   /opt/earthworm/earthworm_7.10/lib/
                   /opt/earthworm/earthworm_7.10/lib64/
                   /opt/earthworm_7.10/lib/
                   /opt/earthworm_7.10/lib64/
                   /usr/local/lib
                   /usr/local/lib64
             )
find_library(Earthworm_UTIL_LIBRARY
             NAMES ew_util
             PATHS $ENV{EW_ROOT}/lib
                   $ENV{EW_ROOT}/lib64
                   $ENV{EW_HOME}/lib
                   $ENV{EW_HOME}/lib64
                   /opt/earthworm/earthworm_7.10/lib/
                   /opt/earthworm/earthworm_7.10/lib64/
                   /opt/earthworm_7.10/lib/
                   /opt/earthworm_7.10/lib64/
                   /usr/local/lib
                   /usr/local/lib64
             )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Earthworm
                                  DEFAULT_MSG Earthworm_INCLUDE_DIR Earthworm_MT_LIBRARY Earthworm_UTIL_LIBRARY)
mark_as_advanced(Earthworm_INCLUDE_DIR Earthworm_MT_LIBRARY Earthworm_UTIL_LIBRARY)
