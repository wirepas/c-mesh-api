# Build platform detection for Wirepas C Mesh API library, GNU make version

# Detect platform
ifeq ($(OS),Windows_NT)
    $(error GNU make on Windows is not supported. \
            Please build with Visual Studio NMAKE)
else
    UNAME := $(shell uname -s)
    ifeq ($(UNAME),Linux)
        $(info Platform: Linux)
        PLATFORM_IS_POSIX := 1
        PLATFORM_IS_LINUX := 1
        CFLAGS += -DPLATFORM_IS_POSIX=1 -DPLATFORM_IS_LINUX=1
    else ifeq ($(UNAME),Darwin)
        $(info Platform: Darwin / macOS)
        PLATFORM_IS_POSIX := 1
        PLATFORM_IS_DARWIN := 1
        CFLAGS += -DPLATFORM_IS_POSIX=1 -DPLATFORM_IS_DARWIN=1
    else
        $(error Platform not recognized)
    endif
endif

# Select toolchain based on platform
ifeq ($(PLATFORM_IS_DARWIN),1)
    # Clang toolchain on Darwin / macOS
    CC  := clang
    AS  := as
    LD  := ld

    # Clang compiler flags
    CFLAGS += -std=gnu99 -Wall -Werror -pthread
    CFLAGS += -Wno-tautological-constant-out-of-range-compare

    # Linker flags
    LDFLAGS +=

    # A command to turn a list of object files into a static library
    ARCHIVE := libtool -static -o
else ifeq ($(PLATFORM_IS_POSIX),1)
    # Standard GCC toolchain
    CC  := gcc
    AS  := as
    LD  := ld

    # GCC compiler flags
    CFLAGS += -std=gnu99 -Wall -Werror -pthread

    # Linker flags
    LDFLAGS += -pthread

    # A command to turn a list of object files into a static library
    ARCHIVE := ar rc -o
endif
