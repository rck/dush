cmake_minimum_required (VERSION 2.6)
project (dush C)

set (PROG_VERSION_MAJOR 0)
set (PROG_VERSION_MINOR 8)

# include binary file in order to find config.h
include_directories("${PROJECT_BINARY_DIR}")

add_executable(dush dush.c)

set (COMMON_FLAGS "-std=c99 -Wall -pedantic")

if (CMAKE_BUILD_TYPE STREQUAL "debug")
   set (DEBUG_FLAGS "-O0 -g")
else()
   set (CMAKE_BUILD_TYPE "release")
endif()
message ("-- Building target: ${CMAKE_BUILD_TYPE}")

set (CMAKE_C_FLAGS_RELEASE "${COMMON_FLAGS}")
set (CMAKE_C_FLAGS_DEBUG "${COMMON_FLAGS} ${DEBUG_FLAGS}")

install (TARGETS dush DESTINATION bin)
install (FILES dush.1 DESTINATION share/man/man1)

include (CheckIncludeFiles)

CHECK_INCLUDE_FILES (uthash.h HAVE_UTHASH_H)

if (NOT HAVE_UTHASH_H)
   message(FATAL_ERROR "Install uthash")
endif()

include (CheckFunctionExists)

CHECK_FUNCTION_EXISTS("getopt_long" HAVE_GETOPT_LONG)

include (CheckTypeSize)

set (CMAKE_EXTRA_INCLUDE_FILES sys/ioctl.h)
CHECK_TYPE_SIZE("struct winsize" HAVE_STRUCT_WINSIZE)

include (CheckSymbolExists)

# first shot, maybe someday other libcs catch up...
CHECK_SYMBOL_EXISTS(FTW_ACTIONRETVAL ftw.h HAVE_ACTIONRETVAL_LIBC)

if (NOT HAVE_ACTIONRETVAL_LIBC) # not in your clib, check for glibc
   message(STATUS "FTW_ACTIONRETVAL not defined, trying -D_GNU_SOURCE")
   set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
   CHECK_SYMBOL_EXISTS(FTW_ACTIONRETVAL ftw.h HAVE_ACTIONRETVAL_GLIBC)
   if (HAVE_ACTIONRETVAL_GLIBC)
      add_definitions(-D_GNU_SOURCE)
   else()
      message(FATAL_ERROR "FTW_ACTIONRETVAL not defined, get glibc")
   endif()
endif()

configure_file (
   "${PROJECT_SOURCE_DIR}/config.h.in"
   "${PROJECT_BINARY_DIR}/config.h"
)

# build a CPack driven installer package
# generall:
include (InstallRequiredSystemLibraries)
set (CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set (CPACK_PACKAGE_CONTACT 
   "Roland Kammerer <dev.rck@gmail.com>")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY 
   "find and display largest files/directories")
set (CPACK_RESOURCE_FILE_LICENSE  
   "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")
set (CPACK_PACKAGE_VERSION_MAJOR "${PROG_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${PROG_VERSION_MINOR}")

# DEB specific
set (CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS "uthash-dev (>=1.8)")

# RPM specific
set (CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set (CPACK_RPM_PACKAGE_LICENSE "GPLv3+")

include (CPack)
