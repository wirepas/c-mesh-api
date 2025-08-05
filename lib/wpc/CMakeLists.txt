add_library(wpc STATIC
    ${CMAKE_CURRENT_LIST_DIR}/attribute.c
    ${CMAKE_CURRENT_LIST_DIR}/csap.c
    ${CMAKE_CURRENT_LIST_DIR}/dsap.c
    ${CMAKE_CURRENT_LIST_DIR}/msap.c
    ${CMAKE_CURRENT_LIST_DIR}/slip.c
    ${CMAKE_CURRENT_LIST_DIR}/wpc.c
    ${CMAKE_CURRENT_LIST_DIR}/wpc_internal.c
    ${CMAKE_CURRENT_LIST_DIR}/reassembly/reassembly.c
)

target_include_directories(wpc PUBLIC
    ${PROJECT_SOURCE_DIR}/api
)

target_include_directories(wpc PRIVATE
    ${PROJECT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_LIST_DIR}/include
)

