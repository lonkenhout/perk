cmake_minimum_required (VERSION 3.10)

project (PEARS)

# source dirs for proj and baseline program
set(PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
#set(BL_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/bl_prog)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# specify the CXX standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# setup flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werr -O3 -Ofast -funroll-loops -march=native")

# find and link libs
find_library(IBVERBS_LIBRARY ibverbs HINTS /home/atr/local/lib)
find_library(RDMACM_LIBRARY rdmacm HINTS /home/atr/local/lib)

link_libraries(pthread ${IBVERBS_LIBRARY} ${RDMACM_LIBRARY})

# check for pkg config and add glib
find_package(PkgConfig)
pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.36.0)
include_directories(${GLIB2_INCLUDE_DIRS})

# add executables
add_executable(pears_server ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/server.c)
add_executable(pears_client ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/client.c)


#add_executable(bl_prog ${BL_SOURCE_DIR}/bl_prog.c)

# add glib2 for it to be linked
target_link_libraries(pears_server ${GLIB2_LIBRARIES})
target_link_libraries(pears_client ${GLIB2_LIBRARIES})
