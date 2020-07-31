cmake_minimum_required (VERSION 2.6)

project (file_system C)

# Ansilove library
find_path(ANSILOVE_INCLUDE_DIRS ansilove.h)
find_library(ANSILOVE_LIBRARIES NAMES ansilove REQUIRED)
include_directories(${ANSILOVE_INCLUDE_DIRS})

find_path(MAGIC_INCLUDE_DIRS magic.h)
find_library(MAGIC_LIBRARIES NAMES magic REQUIRED)
include_directories(${MAGIC_INCLUDE_DIRS})

find_path(FUSE_INCLUDE_DIRS fuse.h)
find_library(FUSE_LIBRARIES NAMES fuse REQUIRED)
include_directories(${FUSE_INCLUDE_DIRS})

set(SRC file_system.c)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64")



add_definitions( -lmagic -lfuse -g -ggdb)
add_executable(file_system ${SRC})

target_link_libraries(file_system ${ANSILOVE_LIBRARIES} ${MAGIC_LIBRARIES} ${FUSE_LIBRARIES})
