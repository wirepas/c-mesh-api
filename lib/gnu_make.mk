# Makefile for Wirepas C Mesh API library, GNU make version

# Variables

# Detect platform and set toolchain variables
include platform.mk

# Path of source files and build outputs
SOURCEPREFIX := .
BUILDPREFIX  := build

# Uncomment the following line only if running with an old stack
#CFLAGS  += -DLEGACY_APP_CONFIG

# Targets definition
LIB_NAME := mesh_api_lib
TARGET_LIB := $(BUILDPREFIX)/$(LIB_NAME).a

SOURCES :=

# Add API header (including logger) and internal headers
CFLAGS += -Iapi -Iinclude

# Include platform module sources
include platform/gnu_make.mk

# Include WPC generic sources
include wpc/gnu_make.mk

OBJECTS := $(patsubst $(SOURCEPREFIX)/%,                     \
                      $(BUILDPREFIX)/%,                      \
                      $(SOURCES:.c=.o))


# Functions

# Use GCC to automatically generate dependency files for C files.
# Also create the target directory if it does not exist
define COMPILE
	echo "  CC $(2)"
	mkdir -p $(dir $(1))
	$(CC) $(CFLAGS) -c -o $(1) $(2)
endef

define LINK_LIB
	echo "  Linking $(1)"
	$(ARCHIVE) $(1) $(2)
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
all: lib

lib: $(TARGET_LIB)

.PHONY: clean
clean:
	$(call CLEAN)

$(BUILDPREFIX)/%.o: $(SOURCEPREFIX)/%.c
	$(call COMPILE,$@,$<)

$(TARGET_LIB): $(OBJECTS)
	$(call LINK_LIB,$@,$^)
