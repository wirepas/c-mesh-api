cmake_minimum_required(VERSION 3.18)

project(meshAPItest LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Werror -Wextra)
option(WPC_ENABLE_COVERAGE "Enable coverage instrumentation for unit-test builds" OFF)

set(WPC_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/../lib/")
set(WPC_INTERNAL_INCLUDE_DIRS
    ${WPC_LIB_DIR}/api
    ${WPC_LIB_DIR}/platform
    ${WPC_LIB_DIR}/wpc/include
)

function(enable_coverage_if_requested target_name)
    if(NOT WPC_ENABLE_COVERAGE)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE -O0 -g --coverage)
        target_link_options(${target_name} PRIVATE --coverage)
    else()
        message(WARNING "WPC_ENABLE_COVERAGE is only supported with GCC or Clang")
    endif()
endfunction()

include(FetchContent)
FetchContent_Declare(
    wpc-lib
    SOURCE_DIR ${WPC_LIB_DIR}
)
set(BUILD_GMOCK OFF)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
)
FetchContent_MakeAvailable(wpc-lib googletest)

enable_coverage_if_requested(wpc)
enable_coverage_if_requested(wpc_platform)

enable_testing()

add_executable(${CMAKE_PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/test_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wpc_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/general_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/scratchpad_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/callback_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/cdd_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ssr_tests.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    GTest::gtest
    wpc
)

# Expose internal WPC headers needed by SSR unit tests.
# (fht.h and ssr.h live in lib/wpc/include/ which is PRIVATE to the wpc lib.)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${WPC_LIB_DIR}/wpc/include
)

include(GoogleTest)
gtest_discover_tests(${CMAKE_PROJECT_NAME})

add_executable(meshAPI_ssr_unit
    ${WPC_LIB_DIR}/wpc/dsap.c
    ${WPC_LIB_DIR}/wpc/msap.c
    ${WPC_LIB_DIR}/wpc/reassembly/reassembly.c
    ${WPC_LIB_DIR}/wpc/ssr/fht.c
    ${WPC_LIB_DIR}/wpc/ssr/ssr.c
    ${CMAKE_CURRENT_LIST_DIR}/ssr_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ssr_routing_unit_tests.cpp
)

target_link_libraries(meshAPI_ssr_unit
    GTest::gtest_main
)

target_include_directories(meshAPI_ssr_unit PRIVATE
    ${WPC_INTERNAL_INCLUDE_DIRS}
)

target_compile_options(meshAPI_ssr_unit PRIVATE -ffunction-sections -fdata-sections)
target_link_options(meshAPI_ssr_unit PRIVATE -Wl,--gc-sections)

enable_coverage_if_requested(meshAPI_ssr_unit)
gtest_discover_tests(meshAPI_ssr_unit)

add_executable(meshAPI_wpc_unit
    ${WPC_LIB_DIR}/wpc/wpc.c
    ${WPC_LIB_DIR}/wpc/wpc_ssr.c
    ${CMAKE_CURRENT_LIST_DIR}/wpc_ssr_unit_tests.cpp
)

target_link_libraries(meshAPI_wpc_unit
    GTest::gtest_main
)

target_include_directories(meshAPI_wpc_unit PRIVATE
    ${WPC_INTERNAL_INCLUDE_DIRS}
)

target_compile_options(meshAPI_wpc_unit PRIVATE -ffunction-sections -fdata-sections)
target_link_options(meshAPI_wpc_unit PRIVATE -Wl,--gc-sections)

enable_coverage_if_requested(meshAPI_wpc_unit)
gtest_discover_tests(meshAPI_wpc_unit)

add_executable(meshAPI_platform_unit
    ${CMAKE_CURRENT_LIST_DIR}/platform_ssr_unit_tests.cpp
)

target_link_libraries(meshAPI_platform_unit
    GTest::gtest_main
    wpc_platform
)

target_include_directories(meshAPI_platform_unit PRIVATE
    ${WPC_INTERNAL_INCLUDE_DIRS}
)

enable_coverage_if_requested(meshAPI_platform_unit)
gtest_discover_tests(meshAPI_platform_unit)

add_executable(meshAPI_wpc_internal_unit
    ${WPC_LIB_DIR}/wpc/wpc_internal.c
    ${CMAKE_CURRENT_LIST_DIR}/wpc_internal_ssr_unit_tests.cpp
)

target_link_libraries(meshAPI_wpc_internal_unit
    GTest::gtest_main
)

target_include_directories(meshAPI_wpc_internal_unit PRIVATE
    ${WPC_INTERNAL_INCLUDE_DIRS}
)

target_compile_options(meshAPI_wpc_internal_unit PRIVATE -ffunction-sections -fdata-sections)
target_link_options(meshAPI_wpc_internal_unit PRIVATE -Wl,--gc-sections)

enable_coverage_if_requested(meshAPI_wpc_internal_unit)
gtest_discover_tests(meshAPI_wpc_internal_unit)
