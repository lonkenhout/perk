cmake_minimum_required (VERSION 3.10)

# add find script to find libmemcached
SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_find" ${CMAKE_MODULE_PATH})

project (PERK)

# source dirs for proj and baseline program
set(PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# specify the CXX standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# setup flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werr -O3 -Ofast -ffast-math -Og -ggdb -funroll-loops -march=native")

# find and link libs
find_library(IBVERBS_LIBRARY ibverbs HINTS /home/lot230/local/lib)
find_library(RDMACM_LIBRARY rdmacm HINTS /home/lot230/local/lib)


link_libraries(pthread ${IBVERBS_LIBRARY} ${RDMACM_LIBRARY})

# check for pkg config and add glVib
find_package(PkgConfig)
#pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.36.0)
#include_directories(${GLIB2_INCLUDE_DIRS})

# check for libmemcached manually and add glib and libmemcached libraries to used libs
pkg_check_modules(LIBMEMCACHED QUIET libmemcached>=1.0.18)
if(NOT LIBMEMCACHED_FOUND)
	message("-- Searching manually for libmemcached")
	find_package(Libmemcached REQUIRED)
	include_directories(${LIBMEMCACHED_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${GLIB2_LIBRARIES} ${LIBMEMCACHED_LIBRARIES})
else()
	message("-- Using system memcached")
	include_directories(${LIBMEMCACHED_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${GLIB2_LIBRARIES} ${LIBMEMCACHED_LIBRARIES})
endif()

if($ENV{PERK_DEBUG})
	message("---- Setting up debugging definitions")
	add_compile_definitions( PERK_DEBUG=1 )
endif()
if($ENV{PERK_BM_LATENCY})
	message("---- Setting up latency benchmarking definitions")
	add_compile_definitions( BM_LATENCY=1 )
endif()
if($ENV{PERK_BM_OPS_PER_SEC})
	message("---- Setting up ops/sec benchmark definitions")
	add_compile_definitions( BM_OPS_PER_SEC=1 )
endif()
if($ENV{PERK_BM_SERVER_EXIT})
	message("---- Setting up definitions for server exit after clients disconnect")
	add_compile_definitions( BM_SERVER_EXIT=1 )
endif()
if($ENV{PERK_PRINT_REQUESTS})
	message("---- Setting up definitions for client side request printing")
	add_compile_definitions( PERK_PRINT_REQUESTS=1 )
endif()
if($ENV{PERK_OVERRIDE_VALSIZE})
    message("---- Overriding PERK value size")
    add_compile_definitions( PERK_OVERRIDE_VALSIZE=$ENV{PERK_OVERRIDE_VALSIZE} )
	message("---- Set val size to: $ENV{PERK_OVERRIDE_VALSIZE}")
endif()

# setup thats runs all other setups
add_executable(perk_server ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/server.c ${PROJECT_SOURCE_DIR}/workers.c ${PROJECT_SOURCE_DIR}/ck_hash.c)
add_executable(perk_client ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/client.c ${PROJECT_SOURCE_DIR}/clients.c)


# client that can connect to memcached server
add_executable(client_mcd ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/client_mcd.c)

# other setup
target_link_libraries(perk_server ${LIBS})
target_link_libraries(perk_client ${LIBS})


# memcached client
target_link_libraries(client_mcd ${LIBS})
