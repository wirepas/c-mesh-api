# Sources for platform module
PLATFORM_MODULE = $(SOURCEPREFIX)/platform

ifeq ($(PLATFORM_IS_POSIX),1)
    # POSIX modules that support Linux, Darwin (macOS), the various BSDs, ...
    SOURCES += $(PLATFORM_MODULE)/posix/logger.c
    SOURCES += $(PLATFORM_MODULE)/posix/platform.c
    SOURCES += $(PLATFORM_MODULE)/posix/serial_posix.c
endif

ifeq ($(PLATFORM_IS_LINUX),1)
    # Linux-specific modules
    SOURCES += $(PLATFORM_MODULE)/linux/serial_linux.c
endif

ifeq ($(PLATFORM_IS_DARWIN),1)
    # Darwin-specific modules
    SOURCES += $(PLATFORM_MODULE)/darwin/serial_darwin.c
endif

CFLAGS  += -I$(PLATFORM_MODULE)
