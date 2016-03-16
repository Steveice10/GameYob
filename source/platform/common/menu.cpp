#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sstream>

#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#include "platform/common/cheatengine.h"
#include "platform/common/cheats.h"
#include "platform/common/config.h"
#include "platform/common/filechooser.h"
#include "platform/common/gbsplayer.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"
#include "apu.h"
#include "gameboy.h"
#include "ppu.h"
#include "printer.h"
#include "romfile.h"

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

void subMenuGenericUpdateFunc(); // Private function here

bool consoleDebugOutput = false;
bool menuOn = false;
int menu = 0;
int option = -1;
char printMessage[33];
int gameScreen = 0;
int pauseOnMenu = 0;
int stateNum = 0;

int borderSetting = 0;

int scaleMode = 0;
int scaleFilter = 0;
int borderScaleMode = 0;

void (*subMenuUpdateFunc)();

bool fpsOutput = false;
bool timeOutput = false;

bool autoSaveEnabled = false;

int gbColorizeMode = 0;
u16 gbBgPalette[0x20];
u16 gbSprPalette[0x20];

bool printerEnabled = 0;

bool soundEnabled = 0;

int sgbModeOption = 0;
int gbcModeOption = 0;
bool gbaModeOption = 0;

int biosMode = 0;

int fastForwardFrameSkip = 0;

FileChooser borderChooser("/", {"png", "bmp"}, true);
FileChooser biosChooser("/", {"bin"}, true);

// Private function used for simple submenus
void subMenuGenericUpdateFunc() {
    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if(key == UI_KEY_A || key == UI_KEY_B) {
            closeSubMenu();
            return;
        }
    }
}

// Functions corresponding to menu options

void suspendFunc(int value) {
    if(gameboy->isRomLoaded() && gameboy->romFile->getRamBanks() > 0) {
        printMenuMessage("Saving SRAM...");
        mgrSave();
    }

    printMenuMessage("Saving state...");
    mgrSaveState(-1);
    printMessage[0] = '\0';

    closeMenu();
    gameboy->unloadRom();
}

void exitFunc(int value) {
    if(gameboy->isRomLoaded() && gameboy->romFile->getRamBanks() > 0) {
        printMenuMessage("Saving SRAM...");
        mgrSave();
    }

    printMessage[0] = '\0';

    closeMenu();
    gameboy->unloadRom();
}

void exitNoSaveFunc(int value) {
    closeMenu();
    gameboy->unloadRom();
}

void consoleOutputFunc(int value) {
    if(value == 0) {
        fpsOutput = false;
        timeOutput = false;
        consoleDebugOutput = false;
    } else if(value == 1) {
        fpsOutput = true;
        timeOutput = false;
        consoleDebugOutput = false;
    } else if(value == 2) {
        fpsOutput = false;
        timeOutput = true;
        consoleDebugOutput = false;
    } else if(value == 3) {
        fpsOutput = true;
        timeOutput = true;
        consoleDebugOutput = false;
    } else if(value == 4) {
        fpsOutput = false;
        timeOutput = false;
        consoleDebugOutput = true;
    }
}

void returnToLauncherFunc(int value) {
    systemRequestExit();
}

void printerEnableFunc(int value) {
    printerEnabled = (bool) value;
}

void cheatFunc(int value) {
    if(cheatEngine != NULL && cheatEngine->getNumCheats() != 0) {
        startCheatMenu();
    } else {
        printMenuMessage("No cheats found!");
    }
}

void keyConfigFunc(int value) {
    startKeyConfigChooser();
}

void saveSettingsFunc(int value) {
    printMenuMessage("Saving settings...");
    writeConfigFile();
    printMenuMessage("Settings saved.");
}

void stateSelectFunc(int value) {
    stateNum = value;
    if(mgrStateExists(stateNum)) {
        enableMenuOption("Load State");
        enableMenuOption("Delete State");
    } else {
        disableMenuOption("Load State");
        disableMenuOption("Delete State");
    }
}

void stateSaveFunc(int value) {
    printMenuMessage("Saving state...");
    if(mgrSaveState(stateNum)) {
        printMenuMessage("State saved.");
    } else {
        printMenuMessage("Could not saveImage state.");
    }

    // Will activate the other state options
    stateSelectFunc(stateNum);
}

void stateLoadFunc(int value) {
    if(!mgrStateExists(stateNum)) {
        printMenuMessage("State does not exist.");
    }

    printMenuMessage("Loading state...");
    if(mgrLoadState(stateNum)) {
        closeMenu();
        printMessage[0] = '\0';
    } else {
        printMenuMessage("Could not load state.");
    }
}

void stateDeleteFunc(int value) {
    mgrDeleteState(stateNum);

    // Will grey out the other state options
    stateSelectFunc(stateNum);
}

void resetFunc(int value) {
    closeMenu();
    gameboy->reset();

    if(gameboy->isRomLoaded() && gameboy->romFile->isGBS()) {
        gbsPlayerReset();
        gbsPlayerDraw();
    }
}

void returnFunc(int value) {
    closeMenu();
}

void gameboyModeFunc(int value) {
    gbcModeOption = value;
}

void gbaModeFunc(int value) {
    gbaModeOption = (bool) value;
}

void sgbModeFunc(int value) {
    sgbModeOption = value;
}

void biosEnableFunc(int value) {
    biosMode = value;
}

void selectGbBiosFunc(int value) {
    char* filename = biosChooser.chooseFile();
    if(filename != NULL) {
        gbBiosPath = filename;
        free(filename);

        mgrRefreshBios();
    }
}

void selectGbcBiosFunc(int value) {
    char* filename = biosChooser.chooseFile();
    if(filename != NULL) {
        gbcBiosPath = filename;
        free(filename);

        mgrRefreshBios();
    }
}

void setScreenFunc(int value) {
    gameScreen = value;
    uiUpdateScreen();
}

void setPauseOnMenuFunc(int value) {
    if(value != pauseOnMenu) {
        pauseOnMenu = value;
        if(pauseOnMenu) {
            mgrPause();
        } else {
            mgrUnpause();
        }
    }
}

void setScaleModeFunc(int value) {
    scaleMode = value;

    mgrRefreshBorder();
}

void setScaleFilterFunc(int value) {
    scaleFilter = value;
}

void setBorderScaleModeFunc(int value) {
    borderScaleMode = value;

    mgrRefreshBorder();
}

void setFastForwardFrameSkipFunc(int value) {
    fastForwardFrameSkip = value;
}

void gbColorizeFunc(int value) {
    gbColorizeMode = value;
    const u16* palette = NULL;
    switch(gbColorizeMode) {
        case 0:
            palette = findPalette("GBC - Grayscale");
            break;
        case 1:
            // Don't set the game's palette until we're past the BIOS screen.
            if(!gameboy->biosOn && gameboy->isRomLoaded()) {
                palette = findPalette(gameboy->romFile->getRomTitle().c_str());
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

    memcpy(gbBgPalette, palette, 4 * sizeof(u16));
    memcpy(gbSprPalette, palette + 4, 4 * sizeof(u16));
    memcpy(gbSprPalette + 4 * 4, palette + 8, 4 * sizeof(u16));
}

void selectBorderFunc(int value) {
    char* filename = borderChooser.chooseFile();
    if(filename != NULL) {
        borderPath = filename;
        free(filename);

        mgrRefreshBorder();
    }
}

void borderFunc(int value) {
    borderSetting = value;
    if(borderSetting == 1) {
        enableMenuOption("Select Border");
    } else {
        disableMenuOption("Select Border");
    }

    mgrRefreshBorder();
}

void soundEnableFunc(int value) {
    soundEnabled = (bool) value;
}

void romInfoFunc(int value) {
    if(gameboy->isRomLoaded()) {
        displaySubMenu(subMenuGenericUpdateFunc);

        static const char* mbcNames[] = {"ROM", "MBC1", "MBC2", "MBC3", "MBC5", "MBC7", "MMM01", "HUC1", "HUC3", "CAMERA", "TAMA5"};

        uiClear();
        uiPrint("ROM Title: \"%s\"\n", gameboy->romFile->getRomTitle().c_str());
        uiPrint("CGB: Supported: %d, Required: %d\n", gameboy->romFile->isCgbSupported(), gameboy->romFile->isCgbRequired());
        uiPrint("Cartridge type: %.2x (%s)\n", gameboy->romFile->getRawMBC(), mbcNames[gameboy->romFile->getMBCType()]);
        uiPrint("ROM Size: %.2x (%d banks)\n", gameboy->romFile->getRawRomSize(), gameboy->romFile->getRomBanks());
        uiPrint("RAM Size: %.2x (%d banks)\n", gameboy->romFile->getRawRamSize(), gameboy->romFile->getRamBanks());
        uiFlush();
    }
}

void versionInfoFunc(int value) {
    displaySubMenu(subMenuGenericUpdateFunc);

    uiClear();
    uiPrint("Version: %s\n", VERSION_STRING);
    uiFlush();
}

void setChanEnabled(int chan, int value) {
    gameboy->apu->setChannelEnabled(chan, value == 1);
}

void chan1Func(int value) {
    setChanEnabled(0, value);
}

void chan2Func(int value) {
    setChanEnabled(1, value);
}

void chan3Func(int value) {
    setChanEnabled(2, value);
}

void chan4Func(int value) {
    setChanEnabled(3, value);
}

void setAutoSaveFunc(int value) {
    autoSaveEnabled = (bool) value;

    if(gameboy->isRomLoaded()) {
        if(!autoSaveEnabled && gameboy->romFile->getRamBanks() > 0 && !gameboy->romFile->isGBS()) {
            enableMenuOption("Exit without saving");
        } else {
            disableMenuOption("Exit without saving");
        }
    }
}

int listenSocket = -1;
FILE* linkSocket = NULL;
std::string linkIp = "";

void listenUpdateFunc() {
    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if(key == UI_KEY_A || key == UI_KEY_B) {
            if(listenSocket != -1) {
                close(listenSocket);
                listenSocket = -1;
            }

            closeSubMenu();
            return;
        }
    }

    if(listenSocket != -1 && linkSocket == NULL) {
        linkSocket = systemSocketAccept(listenSocket, &linkIp);
        if(linkSocket != NULL) {
            close(listenSocket);
            listenSocket = -1;

            uiClear();
            uiPrint("Connected to %s.\n", linkIp.c_str());
            uiPrint("Press A or B to continue.\n");
            uiFlush();
        }
    }
}

void listenFunc(int value) {
    displaySubMenu(listenUpdateFunc);

    uiClear();

    if(linkSocket != NULL) {
        uiPrint("Already connected.\n");
        uiPrint("Press A or B to continue.\n");
    } else {
        listenSocket = systemSocketListen(5000);
        if(listenSocket >= 0) {
            uiPrint("Listening for connection...\n");
            uiPrint("Local IP: %s\n", systemGetIP().c_str());
            uiPrint("Press A or B to cancel.\n");
        } else {
            uiPrint("Failed to open socket: %s\n", strerror(errno));
            uiPrint("Press A or B to continue.\n");
        }
    }

    uiFlush();
}

bool connectPerformed = false;
std::string connectIp = "000.000.000.000";
u32 connectSelection = 0;

void drawConnectSelector() {
    uiClear();
    uiPrint("Input IP to connect to:\n");
    uiPrint("%s\n", connectIp.c_str());
    for(u32 i = 0; i < connectSelection; i++) {
        uiPrint(" ");
    }

    uiPrint("^\n");
    uiPrint("Press A to confirm, B to cancel.\n");
    uiFlush();
}

void connectUpdateFunc() {
    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if((connectPerformed && key == UI_KEY_A) || key == UI_KEY_B) {
            connectPerformed = false;
            connectIp = "000.000.000.000";
            connectSelection = 0;

            closeSubMenu();
            return;
        }

        if(!connectPerformed) {
            if(key == UI_KEY_A) {
                std::string trimmedIp = connectIp;

                bool removeZeros = true;
                for(std::string::size_type i = 0; i < trimmedIp.length(); i++) {
                    if(removeZeros && trimmedIp[i] == '0' && i != trimmedIp.length() - 1 && trimmedIp[i + 1] != '.') {
                        trimmedIp.erase(i, 1);
                        i--;
                    } else {
                        removeZeros = trimmedIp[i] == '.';
                    }
                }

                connectPerformed = true;

                uiClear();
                uiPrint("Connecting to %s...\n", trimmedIp.c_str());

                linkSocket = systemSocketConnect(trimmedIp, 5000);
                if(linkSocket != NULL) {
                    linkIp = trimmedIp;;
                    uiPrint("Connected to %s.\n", linkIp.c_str());
                } else {
                    uiPrint("Failed to connect to socket: %s\n", strerror(errno));
                }

                uiPrint("Press A or B to continue.\n");
                uiFlush();
            }

            bool redraw = false;
            if(key == UI_KEY_LEFT && connectSelection > 0) {
                connectSelection--;
                if(connectIp[connectSelection] == '.') {
                    connectSelection--;
                }

                redraw = true;
            }

            if(key == UI_KEY_RIGHT && connectSelection < connectIp.length() - 1) {
                connectSelection++;
                if(connectIp[connectSelection] == '.') {
                    connectSelection++;
                }

                redraw = true;
            }

            if(key == UI_KEY_UP) {
                connectIp[connectSelection]++;
                if(connectIp[connectSelection] > '9') {
                    connectIp[connectSelection] = '0';
                }

                redraw = true;
            }

            if(key == UI_KEY_DOWN) {
                connectIp[connectSelection]--;
                if(connectIp[connectSelection] < '0') {
                    connectIp[connectSelection] = '9';
                }

                redraw = true;
            }

            if(redraw) {
                drawConnectSelector();
            }
        }
    }
}

void connectFunc(int value) {
    displaySubMenu(connectUpdateFunc);
    if(linkSocket != NULL) {
        connectPerformed = true;

        uiClear();
        uiPrint("Already connected.\n");
        uiPrint("Press A or B to continue.\n");
        uiFlush();
    } else {
        drawConnectSelector();
    }
}

void disconnectFunc(int value) {
    displaySubMenu(subMenuGenericUpdateFunc);

    uiClear();

    if(linkSocket != NULL) {
        fclose(linkSocket);
        linkSocket = NULL;
        linkIp = "";

        uiPrint("Disconnected.\n");
    } else {
        uiPrint("Not connected.\n");
    }

    uiPrint("Press A or B to continue.\n");

    uiFlush();
}

void connectionInfoFunc(int value) {
    displaySubMenu(subMenuGenericUpdateFunc);

    uiClear();

    uiPrint("Status: ");
    if(linkSocket != NULL) {
        uiSetTextColor(TEXT_COLOR_GREEN);
        uiPrint("Connected");
        uiSetTextColor(TEXT_COLOR_NONE);
    } else {
        uiSetTextColor(TEXT_COLOR_RED);
        uiPrint("Disconnected");
        uiSetTextColor(TEXT_COLOR_NONE);
    }

    uiPrint("\n");
    uiPrint("IP: %s\n", linkIp.c_str());
    uiPrint("\n");
    uiPrint("Press A or B to continue.\n");
    uiFlush();
}

struct MenuOption {
    const char* name;
    void (* function)(int);
    int numValues;
    const char* values[15];
    int defaultSelection;

    bool enabled;
    int selection;
};

struct SubMenu {
    const char* name;
    int numOptions;
    MenuOption options[13];
};

SubMenu menuList[] = {
        {
                "Game",
                12,
                {
                        {"Exit", exitFunc, 0, {}, 0},
                        {"Reset", resetFunc, 0, {}, 0},
                        {"Suspend", suspendFunc, 0, {}, 0},
                        {"ROM Info", romInfoFunc, 0, {}, 0},
                        {"Version Info", versionInfoFunc, 0, {}, 0},
                        {"State Slot", stateSelectFunc, 10, {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"}, 0},
                        {"Save State", stateSaveFunc, 0, {}, 0},
                        {"Load State", stateLoadFunc, 0, {}, 0},
                        {"Delete State", stateDeleteFunc, 0, {}, 0},
                        {"Manage Cheats", cheatFunc, 0, {}, 0},
                        {"Exit without saving", exitNoSaveFunc, 0, {}, 0},
                        {"Quit to Launcher", returnToLauncherFunc, 0, {}, 0}
                }
        },
        {
                "GameYob",
                5,
                {
                        {"Button Mapping", keyConfigFunc, 0, {}, 0},
                        {"Console Output", consoleOutputFunc, 4, {"Off", "FPS", "Time", "FPS+Time", "Debug"}, 0},
                        {"Autosaving", setAutoSaveFunc, 2, {"Off", "On"}, 0},
                        {"Pause on Menu", setPauseOnMenuFunc, 2, {"Off", "On"}, 0},
                        {"Save Settings", saveSettingsFunc, 0, {}, 0}
                }
        },
        {
                "Gameboy",
                7,
                {
                        {"GB Printer", printerEnableFunc, 2, {"Off", "On"}, 1},
                        {"GBA Mode", gbaModeFunc, 2, {"Off", "On"}, 0},
                        {"GBC Mode", gameboyModeFunc, 3, {"Off", "If Needed", "On"}, 2},
                        {"SGB Mode", sgbModeFunc, 3, {"Off", "Prefer GBC", "Prefer SGB"}, 1},
                        {"BIOS Mode", biosEnableFunc, 4, {"Off", "Auto", "GB", "GBC"}, 1},
                        {"Select GB BIOS", selectGbBiosFunc, 0, {}, 0},
                        {"Select GBC BIOS", selectGbcBiosFunc, 0, {}, 0}
                }
        },
        {
                "Display",
                8,
                {
                        {"Game Screen", setScreenFunc, 2, {"Top", "Bottom"}, 0},
                        {"Scaling", setScaleModeFunc, 5, {"Off", "125%", "150%", "Aspect", "Full"}, 0},
                        {"Scale Filter", setScaleFilterFunc, 3, {"Nearest", "Linear", "Scale2x"}, 1},
                        {"FF Frame Skip", setFastForwardFrameSkipFunc, 4, {"0", "1", "2", "3"}, 3},
                        {"Colorize GB", gbColorizeFunc, 14, {"Off", "Auto", "Inverted", "Pastel Mix", "Red", "Orange", "Yellow", "Green", "Blue", "Brown", "Dark Green", "Dark Blue", "Dark Brown", "Classic Green"}, 1},
                        {"Borders", borderFunc, 3, {"Off", "Custom", "SGB"}, 1},
                        {"Border Scaling", setBorderScaleModeFunc, 2, {"Pre-Scaled", "Scale Base"}, 0},
                        {"Select Border", selectBorderFunc, 0, {}, 0}
                }
        },
        {
                "Sound",
                5,
                {
                        {"Master", soundEnableFunc, 2, {"Off", "On"}, 1},
                        {"Channel 1", chan1Func, 2, {"Off", "On"}, 1},
                        {"Channel 2", chan2Func, 2, {"Off", "On"}, 1},
                        {"Channel 3", chan3Func, 2, {"Off", "On"}, 1},
                        {"Channel 4", chan4Func, 2, {"Off", "On"}, 1}
                }
        },
        /*{
                "Link",
                4,
                {
                        {"Listen", listenFunc, 0, {}, 0},
                        {"Connect", connectFunc, 0, {}, 0},
                        {"Disconnect", disconnectFunc, 0, {}, 0},
                        {"Connection Info", connectionInfoFunc, 0, {}, 0}
                }
        },*/
};

const int numMenus = sizeof(menuList) / sizeof(SubMenu);

void setMenuDefaults() {
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            menuList[i].options[j].selection = menuList[i].options[j].defaultSelection;
            menuList[i].options[j].enabled = true;
            if(menuList[i].options[j].numValues != 0) {
                int selection = menuList[i].options[j].defaultSelection;
                menuList[i].options[j].function(selection);
            }
        }
    }
}

void displayMenu() {
    inputReleaseAll();
    menuOn = true;
    redrawMenu();
}

void closeMenu() {
    inputReleaseAll();
    menuOn = false;

    uiClear();
    uiFlush();

    mgrUnpause();

    if(gameboy->isRomLoaded() && gameboy->romFile->isGBS()) {
        gbsPlayerDraw();
    }
}

bool isMenuOn() {
    return menuOn;
}

// Some helper functions
void menuCursorUp() {
    option--;
    if(option == -1) {
        return;
    }

    if(option < -1) {
        option = menuList[menu].numOptions - 1;
    }
}

void menuCursorDown() {
    option++;
    if(option >= menuList[menu].numOptions) {
        option = -1;
    }
}

int menuGetOptionRow() {
    return option;
}

void menuSetOptionRow(int row) {
    if(row >= menuList[menu].numOptions) {
        row = menuList[menu].numOptions - 1;
    }

    option = row;
}

// Get the number of VISIBLE rows for this platform
int menuGetNumRows() {
    return menuList[menu].numOptions;
}

void redrawMenu() {
    uiClear();

    int width = uiGetWidth();
    int height = uiGetHeight();

    // Top line: submenu
    int pos = 0;
    int nameStart = (width - strlen(menuList[menu].name) - 2) / 2;
    if(option == -1) {
        nameStart -= 2;

        uiSetTextColor(TEXT_COLOR_GREEN);
        uiPrint("<");
        uiSetTextColor(TEXT_COLOR_NONE);
    } else {
        uiPrint("<");
    }

    pos++;
    for(; pos < nameStart; pos++) {
        uiPrint(" ");
    }

    if(option == -1) {
        uiSetTextColor(TEXT_COLOR_YELLOW);
        uiPrint("* ");
        uiSetTextColor(TEXT_COLOR_NONE);

        pos += 2;
    }

    if(option == -1) {
        uiSetTextColor(TEXT_COLOR_YELLOW);
    }

    uiPrint("[%s]", menuList[menu].name);
    uiSetTextColor(TEXT_COLOR_NONE);

    pos += 2 + strlen(menuList[menu].name);
    if(option == -1) {
        uiSetTextColor(TEXT_COLOR_YELLOW);
        uiPrint(" *");
        uiSetTextColor(TEXT_COLOR_NONE);

        pos += 2;
    }

    for(; pos < width - 1; pos++) {
        uiPrint(" ");
    }

    if(option == -1) {
        uiSetTextColor(TEXT_COLOR_GREEN);
        uiPrint(">");
        uiSetTextColor(TEXT_COLOR_NONE);
    } else {
        uiPrint(">");
    }

    uiPrint("\n");

    // Rest of the lines: options
    for(int i = 0; i < menuList[menu].numOptions; i++) {
        if(!menuList[menu].options[i].enabled) {
            uiSetTextColor(TEXT_COLOR_GRAY);
        } else if(option == i) {
            uiSetTextColor(TEXT_COLOR_YELLOW);
        }

        if(menuList[menu].options[i].numValues == 0) {
            for(unsigned int j = 0; j < (width - strlen(menuList[menu].options[i].name)) / 2 - 2; j++) {
                uiPrint(" ");
            }

            if(i == option) {
                uiPrint("* %s *\n", menuList[menu].options[i].name);
            } else {
                uiPrint("  %s  \n", menuList[menu].options[i].name);
            }

            uiPrint("\n");
        } else {
            for(unsigned int j = 0; j < width / 2 - strlen(menuList[menu].options[i].name); j++) {
                uiPrint(" ");
            }

            if(i == option) {
                uiPrint("* ");
                uiPrint("%s  ", menuList[menu].options[i].name);

                if(menuList[menu].options[i].enabled) {
                    uiSetTextColor(TEXT_COLOR_GREEN);
                }

                uiPrint("%s", menuList[menu].options[i].values[menuList[menu].options[i].selection]);

                if(!menuList[menu].options[i].enabled) {
                    uiSetTextColor(TEXT_COLOR_GRAY);
                } else if(option == i) {
                    uiSetTextColor(TEXT_COLOR_YELLOW);
                }

                uiPrint(" *");
            } else {
                uiPrint("  ");
                uiPrint("%s  ", menuList[menu].options[i].name);
                uiPrint("%s", menuList[menu].options[i].values[menuList[menu].options[i].selection]);
            }

            uiPrint("\n\n");
        }

        uiSetTextColor(TEXT_COLOR_NONE);
    }

    // Message at the bottom
    if(printMessage[0] != '\0') {
        int rows = menuGetNumRows();
        int newlines = height - 1 - (rows * 2 + 2) - 1;
        for(int i = 0; i < newlines; i++) {
            uiPrint("\n");
        }

        int spaces = width - 1 - strlen(printMessage);
        for(int i = 0; i < spaces; i++) {
            uiPrint(" ");
        }

        uiPrint("%s\n", printMessage);

        printMessage[0] = '\0';
    }

    uiFlush();
}

// Called each vblank while the menu is on
void updateMenu() {
    if(!isMenuOn())
        return;

    if(subMenuUpdateFunc != 0) {
        subMenuUpdateFunc();
        return;
    }

    bool redraw = false;
    // Get input
    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if(key == UI_KEY_UP) {
            menuCursorUp();
            redraw = true;
        } else if(key == UI_KEY_DOWN) {
            menuCursorDown();
            redraw = true;
        } else if(key == UI_KEY_LEFT) {
            if(option == -1) {
                menu--;
                if(menu < 0) {
                    menu = numMenus - 1;
                }
            } else if(menuList[menu].options[option].numValues != 0 && menuList[menu].options[option].enabled) {
                int selection = menuList[menu].options[option].selection - 1;
                if(selection < 0) {
                    selection = menuList[menu].options[option].numValues - 1;
                }

                menuList[menu].options[option].selection = selection;
                menuList[menu].options[option].function(selection);
            }

            redraw = true;
        } else if(key == UI_KEY_RIGHT) {
            if(option == -1) {
                menu++;
                if(menu >= numMenus) {
                    menu = 0;
                }
            } else if(menuList[menu].options[option].numValues != 0 && menuList[menu].options[option].enabled) {
                int selection = menuList[menu].options[option].selection + 1;
                if(selection >= menuList[menu].options[option].numValues) {
                    selection = 0;
                }

                menuList[menu].options[option].selection = selection;
                menuList[menu].options[option].function(selection);
            }
            redraw = true;
        } else if(key == UI_KEY_A) {
            if(option >= 0 && menuList[menu].options[option].numValues == 0 && menuList[menu].options[option].enabled) {
                menuList[menu].options[option].function(menuList[menu].options[option].selection);
            }

            redraw = true;
        } else if(key == UI_KEY_B) {
            closeMenu();
        } else if(key == UI_KEY_L) {
            int row = menuGetOptionRow();
            menu--;
            if(menu < 0) {
                menu = numMenus - 1;
            }

            menuSetOptionRow(row);
            redraw = true;
        } else if(key == UI_KEY_R) {
            int row = menuGetOptionRow();
            menu++;
            if(menu >= numMenus) {
                menu = 0;
            }

            menuSetOptionRow(row);
            redraw = true;
        }
    }

    if(redraw && subMenuUpdateFunc == 0 && isMenuOn()) {// The menu may have been closed by an option
        redrawMenu();
    }
}

// Message will be printed immediately, but also stored in case it's overwritten
// right away.
void printMenuMessage(const char* s) {
    int width = uiGetWidth();
    int height = uiGetHeight();
    int rows = menuGetNumRows();

    bool hadPreviousMessage = printMessage[0] != '\0';
    strncpy(printMessage, s, 33);

    if(hadPreviousMessage) {
        uiPrint("\r");
    } else {
        int newlines = height - 1 - (rows * 2 + 2) - 1;
        for(int i = 0; i < newlines; i++) {
            uiPrint("\n");
        }
    }

    int spaces = width - 1 - strlen(printMessage);
    for(int i = 0; i < spaces; i++) {
        uiPrint(" ");
    }

    uiPrint("%s", printMessage);
    uiFlush();
}

void displaySubMenu(void (* updateFunc)()) {
    subMenuUpdateFunc = updateFunc;
}

void closeSubMenu() {
    subMenuUpdateFunc = NULL;
    redrawMenu();
}

int getMenuOption(const char* optionName) {
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                return menuList[i].options[j].selection;
            }
        }
    }

    return 0;
}

void setMenuOption(const char* optionName, int value) {
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                menuList[i].options[j].selection = value;
                menuList[i].options[j].function(value);
                return;
            }
        }
    }
}

void enableMenuOption(const char* optionName) {
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                menuList[i].options[j].enabled = true;
                return;
            }
        }
    }
}

void disableMenuOption(const char* optionName) {
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                menuList[i].options[j].enabled = false;
                return;
            }
        }
    }
}

void menuParseConfig(char* line) {
    char* equalsPos = strchr(line, '=');
    if(equalsPos == 0) {
        return;
    }

    *equalsPos = '\0';
    const char* option = line;
    const char* value = equalsPos + 1;
    int val = atoi(value);
    setMenuOption(option, val);
}

const std::string menuPrintConfig() {
    std::stringstream stream;
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(menuList[i].options[j].numValues != 0) {
                stream << menuList[i].options[j].name << "=" << menuList[i].options[j].selection << "\n";
            }
        }
    }

    return stream.str();
}

bool showConsoleDebug() {
    return consoleDebugOutput && !isMenuOn() && !(gameboy->isRomLoaded() && gameboy->romFile->isGBS());
}

