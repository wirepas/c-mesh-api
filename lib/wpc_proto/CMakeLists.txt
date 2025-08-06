set(GENERATED_PROTOS_FOLDER ${CMAKE_CURRENT_LIST_DIR}/gen/generated_protos)
set(INTERNAL_MODULES ${CMAKE_CURRENT_LIST_DIR}/internal_modules)
set(NANO_PB_FOLDER ${CMAKE_CURRENT_LIST_DIR}/deps/nanopb)

add_library(nanopb STATIC
    ${NANO_PB_FOLDER}/pb_common.c
    ${NANO_PB_FOLDER}/pb_decode.c
    ${NANO_PB_FOLDER}/pb_encode.c
)

target_include_directories(nanopb PUBLIC
    ${NANO_PB_FOLDER}
)

target_compile_options(nanopb PRIVATE
    -DPB_ENABLE_MALLOC=1
)

add_library(wpc_proto STATIC
    ${CMAKE_CURRENT_LIST_DIR}/wpc_proto.c
    ${INTERNAL_MODULES}/common.c
    ${INTERNAL_MODULES}/proto_data.c
    ${INTERNAL_MODULES}/proto_config.c
    ${INTERNAL_MODULES}/proto_otap.c
    ${GENERATED_PROTOS_FOLDER}/config_message.pb.c
    ${GENERATED_PROTOS_FOLDER}/data_message.pb.c
    ${GENERATED_PROTOS_FOLDER}/generic_message.pb.c
    ${GENERATED_PROTOS_FOLDER}/otap_message.pb.c
    ${GENERATED_PROTOS_FOLDER}/wp_global.pb.c
)

# TODO the following warnings should be checked
target_compile_options(wpc_proto PRIVATE
    -Wno-stringop-truncation
    -Wno-implicit-fallthrough
)

target_link_libraries(wpc_proto
    nanopb
)

target_include_directories(wpc_proto PUBLIC
    ${PROJECT_SOURCE_DIR}/api
)

target_include_directories(wpc_proto PRIVATE
    ${PROJECT_SOURCE_DIR}/platform
    ${PROJECT_SOURCE_DIR}/wpc/include
    ${INTERNAL_MODULES}
    ${GENERATED_PROTOS_FOLDER}
)

