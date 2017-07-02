cmake_minimum_required( VERSION 2.8 )
project( Simple-Async-IPC )

set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_STANDARD_REQUIRED ON )

include_directories( ${CMAKE_CURRENT_LIST_DIR} )

set( LIBRARY_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE PATH "Relative or absolute path to directory where built shared libraries will be placed" )

set( USE_IP_LEGACY false CACHE BOOL "Enable to compile for older systems, with no modern socket options (e.g. IPv6)" )

include( ${CMAKE_CURRENT_LIST_DIR}/threads/CMakeLists.txt )

add_library( AsyncIPC SHARED ${CMAKE_CURRENT_LIST_DIR}/ipc.c ${CMAKE_CURRENT_LIST_DIR}/ipc_base_ip.c )
set_target_properties( AsyncIPC PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_DIR} )
target_link_libraries( AsyncIPC MultiThreading )
if( UNIX AND NOT APPLE )
  target_link_libraries( AsyncIPC -lrt )
endif()
target_compile_definitions( AsyncIPC PUBLIC -D_DEFAULT_SOURCE=__STRICT_ANSI__ -DDEBUG )
if( USE_IP_LEGACY )
  target_compile_definitions( AsyncIPC PUBLIC -DIP_NETWORK_LEGACY )
endif()

