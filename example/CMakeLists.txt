cmake_minimum_required(VERSION 3.18)

project(meshAPIExample LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

add_compile_options(-Wall -Werror)

set(WPC_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/../lib/")

include(FetchContent)
FetchContent_Declare(
    wpc-lib
    SOURCE_DIR ${WPC_LIB_DIR}
)
FetchContent_MakeAvailable(wpc-lib)

add_executable(${CMAKE_PROJECT_NAME} main.c)
target_link_libraries(${CMAKE_PROJECT_NAME} wpc)

