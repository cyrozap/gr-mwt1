INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_MWT1 mwt1)

FIND_PATH(
    MWT1_INCLUDE_DIRS
    NAMES mwt1/api.h
    HINTS $ENV{MWT1_DIR}/include
        ${PC_MWT1_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREEFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    MWT1_LIBRARIES
    NAMES gnuradio-mwt1
    HINTS $ENV{MWT1_DIR}/lib
        ${PC_MWT1_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MWT1 DEFAULT_MSG MWT1_LIBRARIES MWT1_INCLUDE_DIRS)
MARK_AS_ADVANCED(MWT1_LIBRARIES MWT1_INCLUDE_DIRS)
