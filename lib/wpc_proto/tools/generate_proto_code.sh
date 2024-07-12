#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
WPC_FOLDER=$SCRIPT_DIR/..

PROTO_FOLDER=$WPC_FOLDER/deps/backend-apis/gateway_to_backend/protocol_buffers_files/
PROTO_OPTIONS_FOLDER=$WPC_FOLDER/proto_options/
NANOPB_FOLDER=$WPC_FOLDER/deps/nanopb/

cd $WPC_FOLDER/gen

mkdir -p proto_files generated_protos

rm -f generated_protos/* proto_files/*

# Copy original proto files
cp ${PROTO_FOLDER}config_message.proto proto_files/
cp ${PROTO_FOLDER}data_message.proto proto_files/
cp ${PROTO_FOLDER}error.proto proto_files/
cp ${PROTO_FOLDER}generic_message.proto proto_files/
cp ${PROTO_FOLDER}otap_message.proto proto_files/
cp ${PROTO_FOLDER}wp_global.proto proto_files/

# Modify package name for shorter generated names
sed -i 's/package wirepas.proto.gateway_api;/package wp;/g' proto_files/*.proto

# Copy Option files alongside
cp ${PROTO_OPTIONS_FOLDER}* proto_files/

# Generate code
python3 ${NANOPB_FOLDER}generator/nanopb_generator.py --proto-path=proto_files -D generated_protos `ls proto_files/*.proto`

cd -