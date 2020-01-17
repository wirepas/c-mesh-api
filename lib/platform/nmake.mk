# Sources for platform module
PLATFORM_MODULE = $(SOURCEPREFIX)\platform

# Windows-specific modules
SOURCES = $(SOURCES) $(PLATFORM_MODULE)\win32\logger.c
SOURCES = $(SOURCES) $(PLATFORM_MODULE)\win32\platform.c
SOURCES = $(SOURCES) $(PLATFORM_MODULE)\win32\serial_win32.c

CFLAGS  = $(CFLAGS) /I$(PLATFORM_MODULE)
