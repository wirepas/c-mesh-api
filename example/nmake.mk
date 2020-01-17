# Makefile for Wirepas C Mesh API example, Microsoft NMAKE version

# Variables

# This example needs the C Mesh API library
MESH_LIB_FOLDER = ..\lib
MESH_LIB = $(MESH_LIB_FOLDER)\build\mesh_api_lib.lib

# Detect platform and set toolchain variables
!INCLUDE $(MESH_LIB_FOLDER)\tools_msvc.mk

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
	echo CLEAN
	-del /F $(OBJECTS) $(TARGET_APP) >NUL 2>NUL
	-rmdir /S /Q $(BUILDPREFIX) >NUL 2>NUL

lib:
	cd $(MESH_LIB_FOLDER) && $(MAKE) /nologo /f nmake.mk

.c.obj:
	echo CC $*.c
	-mkdir $(@D) >NUL 2>NUL
	$(CC) $(CFLAGS) /c /Fo$@ $*.c

$(TARGET_APP): $(OBJECTS) $(MESH_LIB)
	echo LINK $@
	-mkdir $(@D) >NUL 2>NUL
	$(CC) $(LDFLAGS) /Fe$@ $**
