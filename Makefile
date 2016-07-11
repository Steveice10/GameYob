# TARGET #

TARGET ?= 3DS
LIBRARY := 0

ifeq ($(TARGET),3DS)
    ifeq ($(strip $(DEVKITPRO)),)
        $(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
    endif

    ifeq ($(strip $(DEVKITARM)),)
        $(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
    endif
endif

# COMMON CONFIGURATION #

NAME := GameYob

BUILD_DIR := build
OUTPUT_DIR := output
INCLUDE_DIRS := include
SOURCE_DIRS := source

EXTRA_OUTPUT_FILES :=

ifeq ($(TARGET),3DS)
	LIBRARY_DIRS := $(DEVKITPRO)/libctru
	LIBRARIES := citro3d ctru m
else
	LIBRARY_DIRS :=
	LIBRARIES := ncurses SDL2 m
endif

BUILD_FLAGS := -O3 -Wno-unused-function -Wno-unused-result
ifeq ($(TARGET),3DS)
	BUILD_FLAGS += -DBACKEND_3DS
else
	BUILD_FLAGS += -DBACKEND_SDL
endif

RUN_FLAGS :=

VERSION_PARTS := $(subst ., ,$(shell git describe --tags --abbrev=0))

VERSION_MAJOR := $(word 1, $(VERSION_PARTS))
VERSION_MINOR := $(word 2, $(VERSION_PARTS))
VERSION_MICRO := $(word 3, $(VERSION_PARTS))

# 3DS CONFIGURATION #

TITLE := $(NAME)
DESCRIPTION := (Super) GameBoy (Color) emulator.
AUTHOR := Drenn, Steveice10
PRODUCT_CODE := CTR-P-GYOB
UNIQUE_ID := 0xF8003

SYSTEM_MODE := 64MB
SYSTEM_MODE_EXT := Legacy

ICON_FLAGS :=

ROMFS_DIR :=
BANNER_AUDIO := meta/audio.wav
BANNER_IMAGE := meta/banner.cgfx
ICON := meta/icon.png

# INTERNAL #

include buildtools/make_base
