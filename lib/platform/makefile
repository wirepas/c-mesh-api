# Sources for platform module
PLATFORM_MODULE = $(SOURCEPREFIX)platform/

CFLAGS  += -I$(PLATFORM_MODULE)

target_platform?=linux

ifeq ($(target_platform),no)
$(warning No plaftform selected)
else ifneq ($(target_platform),)
# Include the target_platform part
include $(PLATFORM_MODULE)$(target_platform)/makefile
else
$(warning No plaftform selected)
endif
