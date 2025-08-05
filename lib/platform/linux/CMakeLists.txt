add_library(wpc_platform STATIC
    ${CMAKE_CURRENT_LIST_DIR}/logger.c
    ${CMAKE_CURRENT_LIST_DIR}/platform.c
    ${CMAKE_CURRENT_LIST_DIR}/serial.c
    ${CMAKE_CURRENT_LIST_DIR}/serial_termios2.c
)

target_include_directories(wpc_platform PRIVATE
    ${PROJECT_SOURCE_DIR}/api
    ${PROJECT_SOURCE_DIR}/platform
    ${PROJECT_SOURCE_DIR}/wpc/include
)

find_package(Threads REQUIRED)
target_link_libraries(wpc_platform PRIVATE Threads::Threads)

