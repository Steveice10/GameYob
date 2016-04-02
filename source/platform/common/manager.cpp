#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <gameboy.h>

#include "platform/common/cheatengine.h"
#include "platform/common/config.h"
#include "platform/common/filechooser.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/common/stb_image.h"
#include "platform/audio.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"
#include "cartridge.h"
#include "gameboy.h"
#include "mmu.h"
#include "ppu.h"
#include "sgb.h"

#define COMPONENT_8_TO_5(c8) (((c8) * 0x1F * 2 + 0xFF) / (0xFF * 2))
#define TOCGB(r, g, b) ((u16) (COMPONENT_8_TO_5(b) << 10 | COMPONENT_8_TO_5(g) << 5 | COMPONENT_8_TO_5(r)))

static const unsigned short p005[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x52, 0xFF, 0x00), TOCGB(0xFF, 0x42, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x52, 0xFF, 0x00), TOCGB(0xFF, 0x42, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x52, 0xFF, 0x00), TOCGB(0xFF, 0x42, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p006[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x9C, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x9C, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x9C, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p007[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p008[] = {
        TOCGB(0xA5, 0x9C, 0xFF), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0x00, 0x63, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xA5, 0x9C, 0xFF), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0x00, 0x63, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xA5, 0x9C, 0xFF), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0x00, 0x63, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p012[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p013[] = {
        TOCGB(0x00, 0x00, 0x00), TOCGB(0x00, 0x84, 0x84), TOCGB(0xFF, 0xDE, 0x00), TOCGB(0xFF, 0xFF, 0xFF),
        TOCGB(0x00, 0x00, 0x00), TOCGB(0x00, 0x84, 0x84), TOCGB(0xFF, 0xDE, 0x00), TOCGB(0xFF, 0xFF, 0xFF),
        TOCGB(0x00, 0x00, 0x00), TOCGB(0x00, 0x84, 0x84), TOCGB(0xFF, 0xDE, 0x00), TOCGB(0xFF, 0xFF, 0xFF)
};

static const unsigned short p016[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xA5, 0xA5, 0xA5), TOCGB(0x52, 0x52, 0x52), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xA5, 0xA5, 0xA5), TOCGB(0x52, 0x52, 0x52), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xA5, 0xA5, 0xA5), TOCGB(0x52, 0x52, 0x52), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p017[] = {
        TOCGB(0xFF, 0xFF, 0xA5), TOCGB(0xFF, 0x94, 0x94), TOCGB(0x94, 0x94, 0xFF), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xA5), TOCGB(0xFF, 0x94, 0x94), TOCGB(0x94, 0x94, 0xFF), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xA5), TOCGB(0xFF, 0x94, 0x94), TOCGB(0x94, 0x94, 0xFF), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p01B[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xCE, 0x00), TOCGB(0x9C, 0x63, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xCE, 0x00), TOCGB(0x9C, 0x63, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xCE, 0x00), TOCGB(0x9C, 0x63, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p100[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xAD, 0xAD, 0x84), TOCGB(0x42, 0x73, 0x7B), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x73, 0x00), TOCGB(0x94, 0x42, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xAD, 0xAD, 0x84), TOCGB(0x42, 0x73, 0x7B), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p10B[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p10D[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x8C, 0x8C, 0xDE), TOCGB(0x52, 0x52, 0x8C), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x8C, 0x8C, 0xDE), TOCGB(0x52, 0x52, 0x8C), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p110[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x84, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p11C[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x63, 0xC5), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x63, 0xC5), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p20B[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p20C[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x8C, 0x8C, 0xDE), TOCGB(0x52, 0x52, 0x8C), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x8C, 0x8C, 0xDE), TOCGB(0x52, 0x52, 0x8C), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xC5, 0x42), TOCGB(0xFF, 0xD6, 0x00), TOCGB(0x94, 0x3A, 0x00), TOCGB(0x4A, 0x00, 0x00)
};

static const unsigned short p300[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xAD, 0xAD, 0x84), TOCGB(0x42, 0x73, 0x7B), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x73, 0x00), TOCGB(0x94, 0x42, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x73, 0x00), TOCGB(0x94, 0x42, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p304[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x00), TOCGB(0xB5, 0x73, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p305[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x52, 0xFF, 0x00), TOCGB(0xFF, 0x42, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p306[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x9C, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p308[] = {
        TOCGB(0xA5, 0x9C, 0xFF), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0x00, 0x63, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0x63, 0x52), TOCGB(0xD6, 0x00, 0x00), TOCGB(0x63, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0x63, 0x52), TOCGB(0xD6, 0x00, 0x00), TOCGB(0x63, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p30A[] = {
        TOCGB(0xB5, 0xB5, 0xFF), TOCGB(0xFF, 0xFF, 0x94), TOCGB(0xAD, 0x5A, 0x42), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0x00, 0x00, 0x00), TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A),
        TOCGB(0x00, 0x00, 0x00), TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A)
};

static const unsigned short p30C[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x8C, 0x8C, 0xDE), TOCGB(0x52, 0x52, 0x8C), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xC5, 0x42), TOCGB(0xFF, 0xD6, 0x00), TOCGB(0x94, 0x3A, 0x00), TOCGB(0x4A, 0x00, 0x00),
        TOCGB(0xFF, 0xC5, 0x42), TOCGB(0xFF, 0xD6, 0x00), TOCGB(0x94, 0x3A, 0x00), TOCGB(0x4A, 0x00, 0x00)
};

static const unsigned short p30D[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x8C, 0x8C, 0xDE), TOCGB(0x52, 0x52, 0x8C), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p30E[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x84, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p30F[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p312[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x84, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x84, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p319[] = {
        TOCGB(0xFF, 0xE6, 0xC5), TOCGB(0xCE, 0x9C, 0x84), TOCGB(0x84, 0x6B, 0x29), TOCGB(0x5A, 0x31, 0x08),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p31C[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x63, 0xC5), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p405[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x52, 0xFF, 0x00), TOCGB(0xFF, 0x42, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x52, 0xFF, 0x00), TOCGB(0xFF, 0x42, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x5A, 0xBD, 0xFF), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0xFF)
};

static const unsigned short p406[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x9C, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x9C, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x5A, 0xBD, 0xFF), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0xFF)
};

static const unsigned short p407[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x5A, 0xBD, 0xFF), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0xFF)
};

static const unsigned short p500[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xAD, 0xAD, 0x84), TOCGB(0x42, 0x73, 0x7B), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x73, 0x00), TOCGB(0x94, 0x42, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x5A, 0xBD, 0xFF), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0xFF)
};

static const unsigned short p501[] = {
        TOCGB(0xFF, 0xFF, 0x9C), TOCGB(0x94, 0xB5, 0xFF), TOCGB(0x63, 0x94, 0x73), TOCGB(0x00, 0x3A, 0x3A),
        TOCGB(0xFF, 0xC5, 0x42), TOCGB(0xFF, 0xD6, 0x00), TOCGB(0x94, 0x3A, 0x00), TOCGB(0x4A, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p502[] = {
        TOCGB(0x6B, 0xFF, 0x00), TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x52, 0x4A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p503[] = {
        TOCGB(0x52, 0xDE, 0x00), TOCGB(0xFF, 0x84, 0x00), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0xFF, 0xFF, 0xFF),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p508[] = {
        TOCGB(0xA5, 0x9C, 0xFF), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0x00, 0x63, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0x63, 0x52), TOCGB(0xD6, 0x00, 0x00), TOCGB(0x63, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0x00, 0x00, 0xFF), TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xFF, 0x7B), TOCGB(0x00, 0x84, 0xFF)
};

static const unsigned short p509[] = {
        TOCGB(0xFF, 0xFF, 0xCE), TOCGB(0x63, 0xEF, 0xEF), TOCGB(0x9C, 0x84, 0x31), TOCGB(0x5A, 0x5A, 0x5A),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x73, 0x00), TOCGB(0x94, 0x42, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p50B[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xFF, 0x7B), TOCGB(0x00, 0x84, 0xFF), TOCGB(0xFF, 0x00, 0x00)
};

static const unsigned short p50C[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x8C, 0x8C, 0xDE), TOCGB(0x52, 0x52, 0x8C), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xC5, 0x42), TOCGB(0xFF, 0xD6, 0x00), TOCGB(0x94, 0x3A, 0x00), TOCGB(0x4A, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x5A, 0xBD, 0xFF), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x00, 0x00, 0xFF)
};

static const unsigned short p50D[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x8C, 0x8C, 0xDE), TOCGB(0x52, 0x52, 0x8C), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p50E[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x84, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p50F[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x84, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p510[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x84, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p511[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x00, 0xFF, 0x00), TOCGB(0x31, 0x84, 0x00), TOCGB(0x00, 0x4A, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p512[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x84, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p514[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0x00), TOCGB(0xFF, 0x00, 0x00), TOCGB(0x63, 0x00, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x84, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p515[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xAD, 0xAD, 0x84), TOCGB(0x42, 0x73, 0x7B), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xAD, 0x63), TOCGB(0x84, 0x31, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p518[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x84, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p51A[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0xFF, 0x00), TOCGB(0x7B, 0x4A, 0x00), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x84, 0x00), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short p51C[] = {
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x7B, 0xFF, 0x31), TOCGB(0x00, 0x63, 0xC5), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0xFF, 0x84, 0x84), TOCGB(0x94, 0x3A, 0x3A), TOCGB(0x00, 0x00, 0x00),
        TOCGB(0xFF, 0xFF, 0xFF), TOCGB(0x63, 0xA5, 0xFF), TOCGB(0x00, 0x00, 0xFF), TOCGB(0x00, 0x00, 0x00)
};

static const unsigned short pCls[] = {
        TOCGB(0x9B, 0xBC, 0x0F), TOCGB(0x8B, 0xAC, 0x0F), TOCGB(0x30, 0x62, 0x30), TOCGB(0x0F, 0x38, 0x0F),
        TOCGB(0x9B, 0xBC, 0x0F), TOCGB(0x8B, 0xAC, 0x0F), TOCGB(0x30, 0x62, 0x30), TOCGB(0x0F, 0x38, 0x0F),
        TOCGB(0x9B, 0xBC, 0x0F), TOCGB(0x8B, 0xAC, 0x0F), TOCGB(0x30, 0x62, 0x30), TOCGB(0x0F, 0x38, 0x0F)
};

struct GbcPaletteEntry {
    const char* title;
    const unsigned short* p;
};

static const GbcPaletteEntry gbcPalettes[] = {
        {"GB - Classic",     pCls},
        {"GBC - Blue",       p518},
        {"GBC - Brown",      p012},
        {"GBC - Dark Blue",  p50D},
        {"GBC - Dark Brown", p319},
        {"GBC - Dark Green", p31C},
        {"GBC - Grayscale",  p016},
        {"GBC - Green",      p005},
        {"GBC - Inverted",   p013},
        {"GBC - Orange",     p007},
        {"GBC - Pastel Mix", p017},
        {"GBC - Red",        p510},
        {"GBC - Yellow",     p51A},
        {"ALLEY WAY",        p008},
        {"ASTEROIDS/MISCMD", p30E},
        {"ATOMIC PUNK",      p30F}, // unofficial ("DYNABLASTER" alt.)
        {"BA.TOSHINDEN",     p50F},
        {"BALLOON KID",      p006},
        {"BASEBALL",         p503},
        {"BOMBERMAN GB",     p31C}, // unofficial ("WARIO BLAST" alt.)
        {"BOY AND BLOB GB1", p512},
        {"BOY AND BLOB GB2", p512},
        {"BT2RAGNAROKWORLD", p312},
        {"DEFENDER/JOUST",   p50F},
        {"DMG FOOTBALL",     p30E},
        {"DONKEY KONG",      p306},
        {"DONKEYKONGLAND",   p50C},
        {"DONKEYKONGLAND 2", p50C},
        {"DONKEYKONGLAND 3", p50C},
        {"DONKEYKONGLAND95", p501},
        {"DR.MARIO",         p20B},
        {"DYNABLASTER",      p30F},
        {"F1RACE",           p012},
        {"FOOTBALL INT'L",   p502}, // unofficial ("SOCCER" alt.)
        {"G&W GALLERY",      p304},
        {"GALAGA&GALAXIAN",  p013},
        {"GAME&WATCH",       p012},
        {"GAMEBOY GALLERY",  p304},
        {"GAMEBOY GALLERY2", p304},
        {"GBWARS",           p500},
        {"GBWARST",          p500}, // unofficial ("GBWARS" alt.)
        {"GOLF",             p30E},
        {"Game and Watch 2", p304},
        {"HOSHINOKA-BI",     p508},
        {"JAMES  BOND  007", p11C},
        {"KAERUNOTAMENI",    p10D},
        {"KEN GRIFFEY JR",   p31C},
        {"KID ICARUS",       p30D},
        {"KILLERINSTINCT95", p50D},
        {"KINGOFTHEZOO",     p30F},
        {"KIRAKIRA KIDS",    p012},
        {"KIRBY BLOCKBALL",  p508},
        {"KIRBY DREAM LAND", p508},
        {"KIRBY'S PINBALL",  p308},
        {"KIRBY2",           p508},
        {"LOLO2",            p50F},
        {"MAGNETIC SOCCER",  p50E},
        {"MANSELL",          p012},
        {"MARIO & YOSHI",    p305},
        {"MARIO'S PICROSS",  p012},
        {"MARIOLAND2",       p509},
        {"MEGA MAN 2",       p50F},
        {"MEGAMAN",          p50F},
        {"MEGAMAN3",         p50F},
        {"METROID2",         p514},
        {"MILLI/CENTI/PEDE", p31C},
        {"MOGURANYA",        p300},
        {"MYSTIC QUEST",     p50E},
        {"NETTOU KOF 95",    p50F},
        {"NEW CHESSMASTER",  p30F},
        {"OTHELLO",          p50E},
        {"PAC-IN-TIME",      p51C},
        {"PENGUIN WARS",     p30F}, // unofficial ("KINGOFTHEZOO" alt.)
        {"PENGUINKUNWARSVS", p30F}, // unofficial ("KINGOFTHEZOO" alt.)
        {"PICROSS 2",        p012},
        {"PINOCCHIO",        p20C},
        {"POKEBOM",          p30C},
        {"POKEMON BLUE",     p10B},
        {"POKEMON GREEN",    p11C},
        {"POKEMON RED",      p110},
        {"POKEMON YELLOW",   p007},
        {"QIX",              p407},
        {"RADARMISSION",     p100},
        {"ROCKMAN WORLD",    p50F},
        {"ROCKMAN WORLD2",   p50F},
        {"ROCKMANWORLD3",    p50F},
        {"SEIKEN DENSETSU",  p50E},
        {"SOCCER",           p502},
        {"SOLARSTRIKER",     p013},
        {"SPACE INVADERS",   p013},
        {"STAR STACKER",     p012},
        {"STAR WARS",        p512},
        {"STAR WARS-NOA",    p512},
        {"STREET FIGHTER 2", p50F},
        {"SUPER BOMBLISS  ", p006}, // unofficial ("TETRIS BLAST" alt.)
        {"SUPER MARIOLAND",  p30A},
        {"SUPER RC PRO-AM",  p50F},
        {"SUPERDONKEYKONG",  p501},
        {"SUPERMARIOLAND3",  p500},
        {"TENNIS",           p502},
        {"TETRIS",           p007},
        {"TETRIS ATTACK",    p405},
        {"TETRIS BLAST",     p006},
        {"TETRIS FLASH",     p407},
        {"TETRIS PLUS",      p31C},
        {"TETRIS2",          p407},
        {"THE CHESSMASTER",  p30F},
        {"TOPRANKINGTENNIS", p502},
        {"TOPRANKTENNIS",    p502},
        {"TOY STORY",        p30E},
        {"TRIP WORLD",       p500}, // unofficial
        {"VEGAS STAKES",     p50E},
        {"WARIO BLAST",      p31C},
        {"WARIOLAND2",       p515},
        {"WAVERACE",         p50B},
        {"WORLD CUP",        p30E},
        {"X",                p016},
        {"YAKUMAN",          p012},
        {"YOSHI'S COOKIE",   p406},
        {"YOSSY NO COOKIE",  p406},
        {"YOSSY NO PANEPON", p405},
        {"YOSSY NO TAMAGO",  p305},
        {"ZELDA",            p511},
};

static const unsigned short* findPalette(const char* title) {
    for(u32 i = 0; i < (sizeof gbcPalettes) / (sizeof gbcPalettes[0]); i++) {
        if(strcmp(gbcPalettes[i].title, title) == 0) {
            return gbcPalettes[i].p;
        }
    }

    return NULL;
}

#define NS_PER_FRAME ((s64) (1000000000.0 / ((double) CYCLES_PER_SECOND / (double) CYCLES_PER_FRAME)))

Gameboy* gameboy = NULL;
CheatEngine* cheatEngine = NULL;

int fastForwardCounter;

static u32 audioBuffer[2048];

static std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds> lastFrameTime;
static bool fastForward;

static std::string romName;

static int fps;
static std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds> lastPrintTime;

static int autoFireCounterA;
static int autoFireCounterB;

static bool emulationPaused;

static FileChooser romChooser("/", {"sgb", "gbc", "cgb", "gb"}, true);
static bool chooserInitialized = false;

void mgrInit() {
    gameboy = new Gameboy();

    gameboy->settings.printDebug = systemPrintDebug;

    gameboy->settings.readTiltX = inputGetMotionSensorX;
    gameboy->settings.readTiltY = inputGetMotionSensorY;
    gameboy->settings.setRumble = systemSetRumble;

    gameboy->settings.getIRState = systemGetIRState;
    gameboy->settings.setIRState = systemSetIRState;

    gameboy->settings.getCameraImage = systemGetCameraImage;

    gameboy->settings.printImage = systemPrintImage;

    gameboy->settings.frameBuffer = gfxGetScreenBuffer();
    gameboy->settings.framePitch = gfxGetScreenPitch();

    gameboy->settings.audioBuffer = audioBuffer;
    gameboy->settings.audioSamples = sizeof(audioBuffer) / sizeof(u32);
    gameboy->settings.audioSampleRate = audioGetSampleRate();

    cheatEngine = new CheatEngine(gameboy);

    fastForwardCounter = 0;

    memset(audioBuffer, 0, sizeof(audioBuffer));

    lastFrameTime = std::chrono::high_resolution_clock::now();
    fastForward = false;

    romName = "";

    fps = 0;
    lastPrintTime = std::chrono::high_resolution_clock::now();

    autoFireCounterA = 0;
    autoFireCounterB = 0;

    emulationPaused = false;

    chooserInitialized = false;
}

void mgrExit() {
    mgrPowerOff();

    if(gameboy != NULL) {
        delete gameboy;
        gameboy = NULL;
    }

    if(cheatEngine != NULL) {
        delete cheatEngine;
        cheatEngine = NULL;
    }
}

void mgrReset() {
    if(gameboy == NULL) {
        return;
    }

    gameboy->powerOff();
    gameboy->powerOn();

    mgrRefreshBorder();
    mgrRefreshPalette();

    audioClear();

    memset(gfxGetScreenBuffer(), 0, gfxGetScreenPitch() * 224 * sizeof(u32));
    gfxDrawScreen();
}

std::string mgrGetRomName() {
    return romName;
}

void mgrPowerOn(const char* romFile) {
    if(gameboy == NULL) {
        return;
    }

    mgrPowerOff();

    if(romFile != NULL) {
        std::ifstream romStream(romFile, std::ios::binary | std::ios::ate);
        if(!romStream.is_open()) {
            systemPrintDebug("Failed to open ROM file: %s\n", strerror(errno));
            return;
        }

        int romSize = (int) romStream.tellg();
        romStream.seekg(0);

        romName = romFile;
        std::string::size_type dot = romName.find_last_of('.');
        if(dot != std::string::npos) {
            romName = romName.substr(0, dot);
        }

        std::ifstream saveStream(romName + ".sav", std::ios::binary | std::ios::ate);
        if(!saveStream.is_open()) {
            systemPrintDebug("Failed to open save file: %s\n", strerror(errno));
        }

        int saveSize = (int) saveStream.tellg();
        saveStream.seekg(0);

        gameboy->cartridge = new Cartridge(romStream, romSize, saveStream, saveSize);

        romStream.close();
        saveStream.close();

        cheatEngine->loadCheats((romName + ".cht").c_str());

        if(mgrStateExists(-1)) {
            mgrLoadState(-1);
            mgrDeleteState(-1);
        }

        enableMenuOption("Suspend");
        enableMenuOption("ROM Info");
        enableMenuOption("State Slot");
        enableMenuOption("Save State");
        if(mgrStateExists(stateNum)) {
            enableMenuOption("Load State");
            enableMenuOption("Delete State");
        } else {
            disableMenuOption("Load State");
            disableMenuOption("Delete State");
        }

        enableMenuOption("Manage Cheats");
        enableMenuOption("Exit without saving");
    } else {
        disableMenuOption("Suspend");
        disableMenuOption("ROM Info");
        disableMenuOption("State Slot");
        disableMenuOption("Save State");
        disableMenuOption("Load State");
        disableMenuOption("Delete State");
        disableMenuOption("Manage Cheats");
        disableMenuOption("Exit without saving");
    }

    mgrReset();
}

void mgrPowerOff(bool save) {
    if(gameboy != NULL && gameboy->isPoweredOn()) {
        if(gameboy->cartridge != NULL) {
            if(save) {
                mgrWriteSave();
            }

            delete gameboy->cartridge;
            gameboy->cartridge = NULL;
        }

        gameboy->powerOff();
    }

    romName = "";

    mgrRefreshBorder();

    memset(gfxGetScreenBuffer(), 0, gfxGetScreenPitch() * 224 * sizeof(u32));
    gfxDrawScreen();
}

void mgrSelectRom() {
    mgrPowerOff();

    if(!chooserInitialized) {
        chooserInitialized = true;

        DIR* dir = opendir(romPath.c_str());
        if(dir) {
            closedir(dir);
            romChooser.setDirectory(romPath);
        }
    }

    char* filename = romChooser.chooseFile();
    if(filename == NULL) {
        systemRequestExit();
        return;
    }

    mgrPowerOn(filename);
    free(filename);
}

void mgrWriteSave() {
    if(gameboy == NULL || gameboy->cartridge == NULL) {
        return;
    }

    std::ofstream stream(romName + ".sav", std::ios::binary);
    if(!stream.is_open()) {
        systemPrintDebug("Failed to open save file: %s\n", strerror(errno));
        return;
    }

    gameboy->cartridge->save(stream);
    stream.close();
}

const std::string mgrGetStateName(int stateNum) {
    std::stringstream nameStream;
    if(stateNum == -1) {
        nameStream << mgrGetRomName() << ".yss";
    } else {
        nameStream << mgrGetRomName() << ".ys" << stateNum;
    }

    return nameStream.str();
}

bool mgrStateExists(int stateNum) {
    if(gameboy == NULL || gameboy->cartridge == NULL) {
        return false;
    }

    std::ifstream stream(mgrGetStateName(stateNum), std::ios::binary);
    if(stream.is_open()) {
        stream.close();
        return true;
    }

    return false;
}

bool mgrLoadState(int stateNum) {
    if(gameboy == NULL || gameboy->cartridge == NULL) {
        return false;
    }

    std::ifstream stream(mgrGetStateName(stateNum), std::ios::binary);
    if(!stream.is_open()) {
        systemPrintDebug("Failed to open state file: %s\n", strerror(errno));
        return false;
    }

    mgrReset();

    bool ret = gameboy->loadState(stream);
    stream.close();
    return ret;
}

bool mgrSaveState(int stateNum) {
    if(gameboy == NULL || gameboy->cartridge == NULL) {
        return false;
    }

    std::ofstream stream(mgrGetStateName(stateNum), std::ios::binary);
    if(!stream.is_open()) {
        systemPrintDebug("Failed to open state file: %s\n", strerror(errno));
        return false;
    }

    bool ret = gameboy->saveState(stream);
    stream.close();
    return ret;
}

void mgrDeleteState(int stateNum) {
    if(gameboy == NULL || gameboy->cartridge == NULL) {
        return;
    }

    remove(mgrGetStateName(stateNum).c_str());
}

void mgrRefreshPalette() {
    if(gameboy == NULL) {
        return;
    }

    const u16* palette = NULL;
    switch(gbColorizeMode) {
        case 0:
            palette = findPalette("GBC - Grayscale");
            break;
        case 1:
            if(gameboy->cartridge != NULL) {
                palette = findPalette(gameboy->cartridge->getRomTitle().c_str());
            }

            if(palette == NULL) {
                palette = findPalette("GBC - Grayscale");
            }

            break;
        case 2:
            palette = findPalette("GBC - Inverted");
            break;
        case 3:
            palette = findPalette("GBC - Pastel Mix");
            break;
        case 4:
            palette = findPalette("GBC - Red");
            break;
        case 5:
            palette = findPalette("GBC - Orange");
            break;
        case 6:
            palette = findPalette("GBC - Yellow");
            break;
        case 7:
            palette = findPalette("GBC - Green");
            break;
        case 8:
            palette = findPalette("GBC - Blue");
            break;
        case 9:
            palette = findPalette("GBC - Brown");
            break;
        case 10:
            palette = findPalette("GBC - Dark Green");
            break;
        case 11:
            palette = findPalette("GBC - Dark Blue");
            break;
        case 12:
            palette = findPalette("GBC - Dark Brown");
            break;
        case 13:
            palette = findPalette("GB - Classic");
            break;
        default:
            palette = findPalette("GBC - Grayscale");
            break;
    }

    memcpy(gameboy->ppu->getBgPalette(), palette, 4 * sizeof(u16));
    memcpy(gameboy->ppu->getSprPalette(), palette + 4, 4 * sizeof(u16));
    memcpy(gameboy->ppu->getSprPalette() + 4 * 4, palette + 8, 4 * sizeof(u16));
}

bool mgrTryRawBorderFile(std::string border) {
    std::ifstream stream(border, std::ios::binary);
    if(stream.is_open()) {
        stream.close();

        int imgWidth;
        int imgHeight;
        int imgDepth;
        u8* image = stbi_load(border.c_str(), &imgWidth, &imgHeight, &imgDepth, STBI_rgb_alpha);
        if(image == NULL || imgDepth != STBI_rgb_alpha) {
            if(image != NULL) {
                stbi_image_free(image);
            }

            systemPrintDebug("Failed to decode image file.\n");
            return false;
        }

        u8* imgData = new u8[imgWidth * imgHeight * sizeof(u32)];
        for(int x = 0; x < imgWidth; x++) {
            for(int y = 0; y < imgHeight; y++) {
                int offset = (y * imgWidth + x) * 4;
                imgData[offset + 0] = image[offset + 3];
                imgData[offset + 1] = image[offset + 2];
                imgData[offset + 2] = image[offset + 1];
                imgData[offset + 3] = image[offset + 0];
            }
        }

        stbi_image_free(image);

        gfxLoadBorder(imgData, imgWidth, imgHeight);
        delete imgData;

        return true;
    }

    return false;
}

static const char* scaleNames[] = {"off", "125", "150", "aspect", "full"};

bool mgrTryBorderFile(std::string border) {
    std::string extension = "";
    std::string::size_type dotPos = border.rfind('.');
    if(dotPos != std::string::npos) {
        extension = border.substr(dotPos);
        border = border.substr(0, dotPos);
    }

    return (borderScaleMode == 0 && mgrTryRawBorderFile(border + "_" + scaleNames[scaleMode] + extension)) || mgrTryRawBorderFile(border + extension);
}

bool mgrTryBorderName(std::string border) {
    for(std::string extension : supportedImages) {
        if(mgrTryBorderFile(border + "." + extension)) {
            return true;
        }
    }

    return false;
}

void mgrRefreshBorder() {
    gfxLoadBorder(NULL, 0, 0);

    if(customBordersEnabled && gameboy != NULL && gameboy->cartridge != NULL) {
        if(!mgrTryBorderName(mgrGetRomName())) {
            mgrTryBorderFile(borderPath);
        }
    }
}

bool mgrGetFastForward() {
    return fastForward || (!isMenuOn() && inputKeyHeld(FUNC_KEY_FAST_FORWARD));
}

void mgrSetFastForward(bool ff) {
    fastForward = ff;
}

void mgrToggleFastForward() {
    fastForward = !fastForward;
}

void mgrPause() {
    emulationPaused = true;
}

void mgrUnpause() {
    emulationPaused = false;
}

bool mgrIsPaused() {
    return emulationPaused;
}

void mgrRun() {
    if(gameboy == NULL) {
        return;
    }

    while(!gameboy->isPoweredOn()) {
        if(!systemIsRunning()) {
            return;
        }

        mgrSelectRom();
    }

    auto time = std::chrono::high_resolution_clock::now();
    if(mgrGetFastForward() || (time - lastFrameTime).count() >= NS_PER_FRAME) {
        lastFrameTime = time;

        inputUpdate();

        if(isMenuOn()) {
            updateMenu();
        } else if(gameboy->isPoweredOn()) {
            u8 buttonsPressed = 0xFF;

            if(inputKeyHeld(FUNC_KEY_UP)) {
                buttonsPressed &= ~GB_UP;
            }

            if(inputKeyHeld(FUNC_KEY_DOWN)) {
                buttonsPressed &= ~GB_DOWN;
            }

            if(inputKeyHeld(FUNC_KEY_LEFT)) {
                buttonsPressed &= ~GB_LEFT;
            }

            if(inputKeyHeld(FUNC_KEY_RIGHT)) {
                buttonsPressed &= ~GB_RIGHT;
            }

            if(inputKeyHeld(FUNC_KEY_A)) {
                buttonsPressed &= ~GB_A;
            }

            if(inputKeyHeld(FUNC_KEY_B)) {
                buttonsPressed &= ~GB_B;
            }

            if(inputKeyHeld(FUNC_KEY_START)) {
                buttonsPressed &= ~GB_START;
            }

            if(inputKeyHeld(FUNC_KEY_SELECT)) {
                buttonsPressed &= ~GB_SELECT;
            }

            if(inputKeyHeld(FUNC_KEY_AUTO_A)) {
                if(autoFireCounterA <= 0) {
                    buttonsPressed &= ~GB_A;
                    autoFireCounterA = 2;
                }

                autoFireCounterA--;
            }

            if(inputKeyHeld(FUNC_KEY_AUTO_B)) {
                if(autoFireCounterB <= 0) {
                    buttonsPressed &= ~GB_B;
                    autoFireCounterB = 2;
                }

                autoFireCounterB--;
            }

            gameboy->sgb->setController(0, buttonsPressed);

            if(inputKeyPressed(FUNC_KEY_SAVE)) {
                mgrWriteSave();
            }

            if(inputKeyPressed(FUNC_KEY_FAST_FORWARD_TOGGLE)) {
                mgrToggleFastForward();
            }

            if((inputKeyPressed(FUNC_KEY_MENU) || inputKeyPressed(FUNC_KEY_MENU_PAUSE))) {
                if(pauseOnMenu || inputKeyPressed(FUNC_KEY_MENU_PAUSE)) {
                    mgrPause();
                }

                mgrSetFastForward(false);
                displayMenu();
            }

            if(inputKeyPressed(FUNC_KEY_SCALE)) {
                setMenuOption("Scaling", (getMenuOption("Scaling") + 1) % 5);
            }

            if(inputKeyPressed(FUNC_KEY_RESET)) {
                mgrReset();
            }

            if(inputKeyPressed(FUNC_KEY_SCREENSHOT) && !isMenuOn()) {
                gfxTakeScreenshot();
            }
        }

        if(gameboy->isPoweredOn() && !emulationPaused) {
            cheatEngine->applyGSCheats();

            gameboy->settings.drawEnabled = !fastForward || fastForwardCounter++ >= fastForwardFrameSkip;
            gameboy->runFrame();

            if(gameboy->settings.soundEnabled) {
                audioPlay(audioBuffer, gameboy->audioSamplesWritten);
            }

            if(gameboy->settings.drawEnabled) {
                fastForwardCounter = 0;
                gfxDrawScreen();
            }
        }

        fps++;

        time = std::chrono::high_resolution_clock::now();
        if(std::chrono::duration_cast<std::chrono::seconds>(time - lastPrintTime).count() > 0) {
            if(!isMenuOn() && !showConsoleDebug() && (fpsOutput || timeOutput)) {
                uiClear();
                int fpsLength = 0;
                if(fpsOutput) {
                    char buffer[16];
                    snprintf(buffer, 16, "FPS: %d", fps);
                    uiPrint("%s", buffer);
                    fpsLength = (int) strlen(buffer);
                }

                if(timeOutput) {
                    time_t timet = (time_t) std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
                    char *timeString = ctime(&timet);
                    for(int i = 0; ; i++) {
                        if(timeString[i] == ':') {
                            timeString += i - 2;
                            break;
                        }
                    }

                    char timeDisplay[6] = {0};
                    strncpy(timeDisplay, timeString, 5);

                    int spaces = uiGetWidth() - (int) strlen(timeDisplay) - fpsLength;
                    for(int i = 0; i < spaces; i++) {
                        uiPrint(" ");
                    }

                    uiPrint("%s", timeDisplay);
                }

                uiPrint("\n");

                uiFlush();
            }

            fps = 0;
            lastPrintTime = time;
        }
    }
}
