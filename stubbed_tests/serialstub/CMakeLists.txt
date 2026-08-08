add_library(serialstub STATIC
    ${CMAKE_CURRENT_LIST_DIR}/serialstub/serial.cpp
    ${CMAKE_CURRENT_LIST_DIR}/serialstub/serial_stub.cpp
    ${CMAKE_CURRENT_LIST_DIR}/serialstub/wpc_frame.cpp
    ${CMAKE_CURRENT_LIST_DIR}/serialstub/misc/crc.cpp
)

target_include_directories(serialstub PUBLIC
    ${PROJECT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_LIST_DIR}
)

set_target_properties(serialstub PROPERTIES LINKER_LANGUAGE CXX)
target_compile_options(serialstub PRIVATE -Wall -Werror -Wextra)

