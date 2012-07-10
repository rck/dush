cmake_minimum_required (VERSION 2.6)
project (dush C)
add_executable(dush dush.c)

install (TARGETS dush DESTINATION bin)

set(CMAKE_C_FLAGS "-std=c99 -Wall -pedantic")

include (CheckIncludeFiles)

CHECK_INCLUDE_FILES (uthash.h HAVE_UTHASH_H)

if (NOT HAVE_UTHASH_H)
   message(FATAL_ERROR "Install uthash")
endif()

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
