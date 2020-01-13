# Makefile for Wirepas C Mesh API example, GNU make version

# This example needs the C Mesh API library
MESH_LIB_FOLDER := ../lib
MESH_LIB := $(MESH_LIB_FOLDER)/build/mesh_api_lib.a

# Detect platform and set toolchain variables
include $(MESH_LIB_FOLDER)/platform.mk

# Path of source files and build outputs
SOURCEPREFIX   := .
BUILDPREFIX    := build

# Targets definition
MAIN_APP := meshAPIExample

TARGET_APP := $(BUILDPREFIX)/$(MAIN_APP)

# Add API header
CFLAGS  += -I$(MESH_LIB_FOLDER)/api

# Source files
SOURCES:= $(SOURCEPREFIX)/main.c

# Object files
OBJECTS := $(patsubst $(SOURCEPREFIX)/%,         \
                      $(BUILDPREFIX)/%,          \
                      $(SOURCES:.c=.o))


# Functions

# Also create the target directory if it does not exist
define COMPILE
	echo "  CC $(2)"
	mkdir -p $(dir $(1))
	$(CC) $(CFLAGS) -c -o $(1) $(2)
endef

define LINK
	echo "  Linking $(1)"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(1) $(2) $(MESH_LIB)
endef

define CLEAN
	echo "  Cleaning up"
	$(RM) -r $(BUILDPREFIX)
endef


# Target rules

ifeq ($(V),1)
	# "V=1" on command line, print commands
else
.SILENT:
	# Default, do not print commands
endif

.PHONY: all
all: app

app: $(TARGET_APP)

.PHONY: clean
clean:
	$(call CLEAN)

$(MESH_LIB):
	make -C $(MESH_LIB_FOLDER)

$(BUILDPREFIX)/%.o: $(SOURCEPREFIX)/%.c
	$(call COMPILE,$@,$<)

$(BUILDPREFIX)/$(MAIN_APP): $(OBJECTS) $(MESH_LIB)
	$(call LINK,$@,$^)
