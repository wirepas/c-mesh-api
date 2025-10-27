cmake_minimum_required(VERSION 3.18)

project(meshAPItest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TEST_EXTRA_CFLAGS "" CACHE STRING "Extra compiler flags for WPC library and test code. For example '--coverage'.")
set(TEST_EXTRA_LDFLAGS "" CACHE STRING "Extra linker flags for the test executable. For example '--coverage'.")
separate_arguments(TEST_EXTRA_CFLAGS)
separate_arguments(TEST_EXTRA_LDFLAGS)

add_compile_options(-Wall -Werror -Wextra)

set(WPC_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/../lib/")

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

enable_testing()

add_executable(${CMAKE_PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/test_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wpc_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/general_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/scratchpad_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/callback_tests.cpp
    ${CMAKE_CURRENT_LIST_DIR}/cdd_tests.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    GTest::gtest
    wpc
)

include(GoogleTest)
gtest_discover_tests(${CMAKE_PROJECT_NAME})

target_compile_options(wpc                   PRIVATE ${TEST_EXTRA_CFLAGS})
target_compile_options(wpc_platform          PRIVATE ${TEST_EXTRA_CFLAGS})
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE ${TEST_EXTRA_CFLAGS})
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE ${TEST_EXTRA_LDFLAGS})

