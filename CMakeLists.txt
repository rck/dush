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

#include (CheckSymbolExists)
#
#CHECK_SYMBOL_EXISTS(FTW_ACTIONRETVAL ftw.h HAVE_ACTIONRETVAL)
#
#if (NOT HAVE_ACTIONRETVAL)
#   message(FATAL_ERROR "You need at least glibc 2.3.3")
#endif()
