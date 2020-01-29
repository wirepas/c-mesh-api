# Toolchain settings for Wirepas C Mesh API library, Microsoft NMAKE version

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
