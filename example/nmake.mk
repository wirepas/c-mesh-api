# Makefile for Wirepas C Mesh API example, Microsoft NMAKE version

# This example needs the C Mesh API library
MESH_LIB_FOLDER = ..\lib
MESH_LIB = $(MESH_LIB_FOLDER)\build\mesh_api_lib.lib

# Microsoft Visual studio command line toolchain on Windows
# Use Visual Studio defaults
#CC  = cl
#AS  = ml

# Visual Studio C compiler flags
CFLAGS = $(CFLAGS) /nologo /W4 /WX /O2 /MD /TC

# Linker flags
LDFLAGS = $(LDFLAGS) /nologo

# Platform flags
!MESSAGE Platform: WIN32
PLATFORM_IS_WIN32 = 1
CFLAGS = $(CFLAGS) /DPLATFORM_IS_WIN32=1

# Path of source files and build outputs
SOURCEPREFIX = .
BUILDPREFIX = build

# Targets definition
MAIN_APP = meshAPIExample
TARGET_APP = $(BUILDPREFIX)\$(MAIN_APP).exe

# Add API header
CFLAGS = $(CFLAGS) /I$(MESH_LIB_FOLDER)\api

# Source files
SOURCES = $(SOURCEPREFIX)\main.c

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

all: app

app: lib $(TARGET_APP)

clean:
	echo   Cleaning up
	-del /F $(OBJECTS) $(TARGET_APP) >NUL 2>NUL
	-rmdir /S /Q $(BUILDPREFIX) >NUL 2>NUL

lib:
	cd $(MESH_LIB_FOLDER) && $(MAKE) /nologo /f nmake.mk

.c.obj:
	-mkdir $(@D) >NUL 2>NUL
	$(CC) $(CFLAGS) /c /Fo$@ $*.c

$(TARGET_APP): $(OBJECTS) $(MESH_LIB)
	echo   Linking $@
	-mkdir $(@D) >NUL 2>NUL
	$(CC) $(LDFLAGS) /Fe$@ $**
