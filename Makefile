# TARGET #

TARGET := SWITCH 3DS NATIVE

# COMMON CONFIGURATION #

NAME := GameYob

BUILD_DIR := build
OUTPUT_DIR := output
INCLUDE_DIRS := include
SOURCE_DIRS := source

BUILD_FLAGS := -O3

VERSION_PARTS := $(subst ., ,$(shell git describe --tags --abbrev=0))

VERSION_MAJOR := $(word 1, $(VERSION_PARTS))
VERSION_MINOR := $(word 2, $(VERSION_PARTS))
VERSION_MICRO := $(word 3, $(VERSION_PARTS))

# PC CONFIGURATION #

ifneq ($(TARGET),$(filter $(TARGET),3DS SWITCH))
    ifeq ($(OS),Windows_NT)
        LIBRARIES += pdcurses SDL2main SDL2 dinput8 dxguid dxerr8 user32 gdi32 winmm imm32 ole32 oleaut32 shell32 version uuid
    else
        LIBRARIES += ncurses SDL2
    endif

    BUILD_FLAGS += -DBACKEND_SDL
endif

# 3DS/Switch CONFIGURATION #

ifeq ($(TARGET),$(filter $(TARGET),3DS SWITCH))
    AUTHOR := Drenn, Steveice10
endif

# 3DS CONFIGURATION #

ifeq ($(TARGET),3DS)
    LIBRARY_DIRS += $(DEVKITPRO)/libctru
    LIBRARIES += citro3d ctru

    BUILD_FLAGS += -DBACKEND_3DS

    DESCRIPTION := (Super) GameBoy (Color) emulator.

    PRODUCT_CODE := CTR-P-GYOB
    UNIQUE_ID := 0xF8003

    BANNER_AUDIO := meta/audio_3ds.wav
    BANNER_IMAGE := meta/banner_3ds.cgfx
    ICON := meta/icon_3ds.png
endif

# SWITCH CONFIGURATION #

ifeq ($(TARGET),SWITCH)
    LIBRARY_DIRS += $(DEVKITPRO)/libnx
    LIBRARIES += nx

    BUILD_FLAGS += -DBACKEND_SWITCH

    ICON := meta/icon_switch.png
endif

# INTERNAL #

include buildtools/make_base
