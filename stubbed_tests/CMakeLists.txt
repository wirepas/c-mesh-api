cmake_minimum_required(VERSION 3.18)

project(c-mesh-api-stubbed-tests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TEST_EXTRA_CFLAGS "" CACHE STRING "Extra compiler flags for WPC library and test code. For example '--coverage'.")
set(TEST_EXTRA_LDFLAGS "" CACHE STRING "Extra linker flags for the test executable. For example '--coverage'.")
separate_arguments(TEST_EXTRA_CFLAGS)
separate_arguments(TEST_EXTRA_LDFLAGS)

set(WPC_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/../lib/")

include(FetchContent)
FetchContent_Declare(
    wpc-lib
    SOURCE_DIR ${WPC_LIB_DIR}
)
FetchContent_Declare(
  fuzztest
  GIT_REPOSITORY https://github.com/google/fuzztest/
  GIT_TAG 2025-08-05
)
FetchContent_MakeAvailable(wpc-lib fuzztest)

enable_testing()

fuzztest_setup_fuzzing_flags()
add_executable(stubbed_tests
    tests/basic_tests.cpp
    tests/ul_fuzz_tests.cpp
    tests/utils/queued_ul_data_handler.cpp
)
target_include_directories(stubbed_tests PRIVATE tests)

add_subdirectory(serialstub)

target_include_directories(serialstub PRIVATE ${WPC_LIB_DIR}/platform)

##########
# Somewhat hacky way to suppress warnings for fuzztest library. This allows us
# to use stricter compile flags for warnings in our testing code that includes
# files from the fuzztest.
get_target_property(LIB_INCLUDE_DIRS fuzztest_fuzztest INTERFACE_INCLUDE_DIRECTORIES)
if(LIB_INCLUDE_DIRS)
    set_target_properties(fuzztest_fuzztest PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${LIB_INCLUDE_DIRS}"
    )
endif()
get_target_property(LIB_INCLUDE_DIRS fuzztest_fuzztest_gtest_main INTERFACE_INCLUDE_DIRECTORIES)
if(LIB_INCLUDE_DIRS)
    set_target_properties(fuzztest_fuzztest_gtest_main PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${LIB_INCLUDE_DIRS}"
    )
endif()
##########

target_compile_options(wpc                   PRIVATE ${TEST_EXTRA_CFLAGS} -Werror -Wall -Wextra)
target_compile_options(wpc_platform          PRIVATE ${TEST_EXTRA_CFLAGS} -Werror -Wall -Wextra)
target_compile_options(stubbed_tests PRIVATE ${TEST_EXTRA_CFLAGS} -Wall -Werror -Wextra)
target_link_options(stubbed_tests PRIVATE ${TEST_EXTRA_LDFLAGS})

# "serialstub" should come before "wpc_platform" so that the serial implementation
# from "wpc_platform" is not taken into use.
target_link_libraries(stubbed_tests PRIVATE
    GTest::gmock
    wpc
    serialstub
    wpc_platform
)

include(GoogleTest)
link_fuzztest(stubbed_tests)
gtest_discover_tests(stubbed_tests)

