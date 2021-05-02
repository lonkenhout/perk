cmake_minimum_required (VERSION 3.10)

# add find script to find libmemcached
SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_find" ${CMAKE_MODULE_PATH})

project (PEARS)

# source dirs for proj and baseline program
set(PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# specify the CXX standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# setup flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werr -O3 -Ofast -funroll-loops -march=native")

# find and link libs
find_library(IBVERBS_LIBRARY ibverbs HINTS /home/lot230/local/lib)
find_library(RDMACM_LIBRARY rdmacm HINTS /home/lot230/local/lib)


link_libraries(pthread ${IBVERBS_LIBRARY} ${RDMACM_LIBRARY})

# check for pkg config and add glib
find_package(PkgConfig)
pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.36.0)
include_directories(${GLIB2_INCLUDE_DIRS})

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


# write/recv setup
add_executable(pears_server_w_sd ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/server_w_sd.c)
add_executable(pears_client_w_sd ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/client_w_sd.c)

# write_imm/recv setup
add_executable(pears_server_wimm_sd ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/server_wimm_sd.c)
add_executable(pears_client_wimm_sd ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/client_wimm_sd.c)

# send/send setup
add_executable(pears_server_sd_sd ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/server_sd_sd.c)
add_executable(pears_client_sd_sd ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/client_sd_sd.c)

# other setup
add_executable(pears_server ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/server.c)
add_executable(pears_client ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/rdma.c ${PROJECT_SOURCE_DIR}/client.c)

add_executable(client_mcd ${PROJECT_SOURCE_DIR}/util.c ${PROJECT_SOURCE_DIR}/client_mcd.c)

# add glib2 for it to be linked
target_link_libraries(pears_server_w_sd ${LIBS})
target_link_libraries(pears_client_w_sd ${LIBS})

target_link_libraries(pears_server_sd_sd ${LIBS})
target_link_libraries(pears_client_sd_sd ${LIBS})

target_link_libraries(pears_server_wimm_sd ${LIBS})
target_link_libraries(pears_client_wimm_sd ${LIBS})

# other setup
target_link_libraries(pears_server ${LIBS})
target_link_libraries(pears_client ${LIBS})

# memcached client
target_link_libraries(client_mcd ${LIBS})
