# Sources for platform module
PLATFORM_MODULE = $(SOURCEPREFIX)platform/linux

SOURCES += $(PLATFORM_MODULE)/platform.c
SOURCES += $(PLATFORM_MODULE)/serial.c
SOURCES += $(PLATFORM_MODULE)/serial_termios2.c
SOURCES += $(PLATFORM_MODULE)/logger.c

# Add the reentrant flag as using pthread lib
CFLAGS  += -D_REENTRANT