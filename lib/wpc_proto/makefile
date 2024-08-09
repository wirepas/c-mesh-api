# Sources for WPC-proto module

WPC_PROTO_MODULE = $(SOURCEPREFIX)wpc_proto/

GENERATED_PROTOS_FOLDER := $(WPC_PROTO_MODULE)gen/generated_protos/
NANO_PB_FOLDER := $(WPC_PROTO_MODULE)deps/nanopb/
INTERNAL_MODULES := $(WPC_PROTO_MODULE)internal_modules/

SOURCES += $(WPC_PROTO_MODULE)wpc_proto.c

CFLAGS += -I$(INTERNAL_MODULES)
SOURCES += $(INTERNAL_MODULES)common.c
SOURCES += $(INTERNAL_MODULES)proto_data.c
SOURCES += $(INTERNAL_MODULES)proto_config.c
SOURCES += $(INTERNAL_MODULES)proto_otap.c

# Add standard proto files
CFLAGS += -I$(NANO_PB_FOLDER)
SOURCES += $(NANO_PB_FOLDER)pb_common.c
SOURCES += $(NANO_PB_FOLDER)pb_decode.c
SOURCES += $(NANO_PB_FOLDER)pb_encode.c

# Add our proto generated files
CFLAGS += -I$(GENERATED_PROTOS_FOLDER)
CFLAGS += -DPB_ENABLE_MALLOC=1
SOURCES += $(GENERATED_PROTOS_FOLDER)config_message.pb.c
SOURCES += $(GENERATED_PROTOS_FOLDER)data_message.pb.c
SOURCES += $(GENERATED_PROTOS_FOLDER)generic_message.pb.c
SOURCES += $(GENERATED_PROTOS_FOLDER)otap_message.pb.c
SOURCES += $(GENERATED_PROTOS_FOLDER)wp_global.pb.c
