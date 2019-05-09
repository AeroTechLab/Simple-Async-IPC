cmake_minimum_required( VERSION 2.8 )
project( Simple-Async-IPC )

set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_STANDARD_REQUIRED ON )

set( LIBRARY_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE PATH "Relative or absolute path to directory where built shared libraries will be placed" )

set( USE_IP_LEGACY false CACHE BOOL "Enable to compile for older systems, with no modern socket options (e.g. IPv6)" )
# set( USE_ZMQ false CACHE BOOL "Use IPC library based on ZeroMQ" )


# Find ZeroMQ libraries
# if( USE_ZMQ )
#   ## load in pkg-config support
#   find_package( PkgConfig )
#   ## use pkg-config to get hints for 0mq locations
#   pkg_check_modules( PC_ZeroMQ QUIET zmq )
#   ## use the hint from above to find where 'zmq.hpp' is located
#   find_path( ZeroMQ_INCLUDE_DIR NAMES zmq.hpp PATHS ${PC_ZeroMQ_INCLUDE_DIRS} )
#   ## use the hint from about to find the location of libzmq
#   find_library( ZeroMQ_LIBRARY NAMES zmq PATHS ${PC_ZeroMQ_LIBRARY_DIRS} )
#   
#   add_library( AsyncIPC SHARED ${CMAKE_CURRENT_LIST_DIR}/ipc.c ${CMAKE_CURRENT_LIST_DIR}/ipc_zmq.c )
#   set_target_properties( AsyncIPC PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_DIR} )
#   target_include_directories( AsyncIPC PUBLIC ${ZeroMQ_INCLUDE_DIR} )
#   target_link_libraries( AsyncIPC ${ZeroMQ_LIBRARY} )
# 
# else()

  if( NOT TARGET MultiThreading )
    include( ${CMAKE_CURRENT_LIST_DIR}/threads/CMakeLists.txt )
  endif()

  add_library( AsyncIPC SHARED ${CMAKE_CURRENT_LIST_DIR}/ipc.c ${CMAKE_CURRENT_LIST_DIR}/ipc_base_ip.c ${CMAKE_CURRENT_LIST_DIR}/ipc_base_shm.c )
  set_target_properties( AsyncIPC PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_DIR} )
  target_include_directories( AsyncIPC PUBLIC ${CMAKE_CURRENT_LIST_DIR} )
  target_link_libraries( AsyncIPC MultiThreading )

  target_compile_definitions( AsyncIPC PUBLIC -D_DEFAULT_SOURCE=__STRICT_ANSI__ -DDEBUG )
  if( USE_IP_LEGACY )
    target_compile_definitions( AsyncIPC PUBLIC -DIP_NETWORK_LEGACY )
  endif()

# endif()
