# Makefile for Wirepas C Mesh API library, Microsoft NMAKE version

# Variables

# Microsoft Visual studio command line toolchain on Windows
# Use Visual Studio defaults
#CC  = cl
#AS  = ml

# Visual Studio C compiler flags
CFLAGS = $(CFLAGS) /nologo /W4 /WX /O2 /MD /TC

# Linker flags
LDFLAGS = $(LDFLAGS)

# Platform flags
!MESSAGE Platform: WIN32
PLATFORM_IS_WIN32 = 1
CFLAGS = $(CFLAGS) /DPLATFORM_IS_WIN32=1

# Path of source files and build outputs
SOURCEPREFIX = .
BUILDPREFIX = build

# Uncomment the following line only if running with an old stack
#CFLAGS  += /DLEGACY_APP_CONFIG=1

# Targets definition
LIB_NAME = mesh_api_lib
TARGET_LIB = $(BUILDPREFIX)\$(LIB_NAME).lib

# Source files
SOURCES =

# Add API header (including logger) and internal headers
CFLAGS = $(CFLAGS) /Iapi /Iinclude

# Include platform module sources
!INCLUDE platform\nmake.mk

# Include WPC generic sources
!INCLUDE wpc\nmake.mk

# Object files, one for each C source file
OBJECTS = $(SOURCES:.c=.obj)


# Target rules

!IF "$(V)"=="1"
	# "V=1" on command line, print commands
!MESSAGE SOURCES: $(SOURCES)
!MESSAGE OBJECTS: $(OBJECTS)
!ELSE
.SILENT:# Default, do not print commands
!ENDIF

all: lib

lib: $(TARGET_LIB)

clean:
	echo   Cleaning up
	-del /F $(OBJECTS) $(TARGET_LIB) >NUL 2>NUL
	-rmdir /S /Q $(BUILDPREFIX) >NUL 2>NUL

.c.obj:
	-mkdir $(@D) >NUL 2>NUL
	$(CC) $(CFLAGS) /c /Fo$@ $*.c

$(TARGET_LIB): $(OBJECTS)
	echo   Linking $@
	-mkdir $(@D) >NUL 2>NUL
	lib /NOLOGO /OUT:$@ $**
