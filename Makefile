ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPRO")
endif

#---------------------------------------------------------------------------------
# BUILD_FLAGS: List of extra build flags to add.
# NO_CTRCOMMON: Do not include ctrcommon.
# ENABLE_EXCEPTIONS: Enable C++ exceptions.
#---------------------------------------------------------------------------------
BUILD_FLAGS := -DVERSION_STRING=\"`git describe --always --abbrev=4`\" -include "3ds/types.h"

include $(DEVKITPRO)/ctrcommon/tools/make_base