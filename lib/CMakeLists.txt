cmake_minimum_required(VERSION 3.18)

project(c-mesh-api LANGUAGES C)

set(WPC_PLATFORM "linux" CACHE STRING "Platform implementation")
option(WPC_BUILD_PROTO_API "Build wpc_proto library" OFF)

add_subdirectory(wpc)

if(${WPC_PLATFORM} STREQUAL "linux")
  add_subdirectory(platform/${WPC_PLATFORM})
endif()

if(WPC_BUILD_PROTO_API)
  add_subdirectory(wpc_proto)
endif()

