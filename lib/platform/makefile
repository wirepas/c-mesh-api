# Sources for platform module
PLATFORM_MODULE = $(SOURCEPREFIX)platform/

# For now, only support linux platform
SOURCES += $(PLATFORM_MODULE)linux/platform.c
SOURCES += $(PLATFORM_MODULE)linux/serial.c
SOURCES += $(PLATFORM_MODULE)linux/serial_termios2.c
SOURCES += $(PLATFORM_MODULE)linux/logger.c

# Add the reentrant flag as using pthread lib
CFLAGS  += -D_REENTRANT

CFLAGS  += -I$(PLATFORM_MODULE)
