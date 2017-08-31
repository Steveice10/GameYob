# TARGET #

TARGET := 3DS
LIBRARY := 0

ifeq ($(TARGET),$(filter $(TARGET),3DS WIIU))
    ifeq ($(strip $(DEVKITPRO)),)
        $(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
    endif
endif

# COMMON CONFIGURATION #

NAME := GameYob

BUILD_DIR := build
OUTPUT_DIR := output
INCLUDE_DIRS := include
SOURCE_DIRS := source

EXTRA_OUTPUT_FILES :=

LIBRARY_DIRS :=
LIBRARIES :=

BUILD_FLAGS := -O3 -Wno-unused-function -Wno-unused-result
BUILD_FLAGS_CC :=
BUILD_FLAGS_CXX :=
RUN_FLAGS :=

VERSION_PARTS := $(subst ., ,$(shell git describe --tags --abbrev=0))

VERSION_MAJOR := $(word 1, $(VERSION_PARTS))
VERSION_MINOR := $(word 2, $(VERSION_PARTS))
VERSION_MICRO := $(word 3, $(VERSION_PARTS))

# PC CONFIGURATION #

ifneq ($(TARGET),$(filter $(TARGET),3DS WIIU))
    ifeq ($(OS),Windows_NT)
        LIBRARIES += pdcurses SDL2main SDL2 dinput8 dxguid dxerr8 user32 gdi32 winmm imm32 ole32 oleaut32 shell32 version uuid
    else
        LIBRARIES += ncurses SDL2
    endif

    BUILD_FLAGS += -DBACKEND_SDL
endif

# 3DS/Wii U CONFIGURATION #

ifeq ($(TARGET),$(filter $(TARGET),3DS WIIU))
    TITLE := $(NAME)
    DESCRIPTION := (Super) GameBoy (Color) emulator.
    AUTHOR := Drenn, Steveice10
endif

# 3DS CONFIGURATION #

ifeq ($(TARGET),3DS)
    LIBRARY_DIRS += $(DEVKITPRO)/libctru
    LIBRARIES += citro3d ctru

    BUILD_FLAGS += -DBACKEND_3DS

    PRODUCT_CODE := CTR-P-GYOB
    UNIQUE_ID := 0xF8003

    CATEGORY := Application
    USE_ON_SD := true

    MEMORY_TYPE := Application
    SYSTEM_MODE := 64MB
    SYSTEM_MODE_EXT := Legacy
    CPU_SPEED := 268MHz
    ENABLE_L2_CACHE := true

    ICON_FLAGS :=

    ROMFS_DIR :=
    BANNER_AUDIO := meta/audio_3ds.wav
    BANNER_IMAGE := meta/banner_3ds.cgfx
    ICON := meta/icon_3ds.png
    LOGO :=
endif

# Wii U CONFIGURATION #

ifeq ($(TARGET),WIIU)
    LIBRARY_DIRS +=
    LIBRARIES +=

    LONG_DESCRIPTION := (Super) GameBoy (Color) emulator.
    ICON := meta/icon_wiiu.png
endif

# INTERNAL #

include buildtools/make_base
