#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gb_apu/Gb_Apu.h"
#include "gb_apu/Multi_Buffer.h"

#include "platform/audio.h"
#include "platform/input.h"
#include "platform/system.h"
#include "cheatengine.h"
#include "gameboy.h"
#include "ppu.h"
#include "printer.h"
#include "romfile.h"

#define GB_A 0x01
#define GB_B 0x02
#define GB_SELECT 0x04
#define GB_START 0x08
#define GB_RIGHT 0x10
#define GB_LEFT 0x20
#define GB_UP 0x40
#define GB_DOWN 0x80

#define MAX_WAIT_CYCLES 1000000

#define TO5BIT(c8) (((c8) * 0x1F * 2 + 0xFF) / (0xFF * 2))
#define TOCGB(r, g, b) (TO5BIT(b) << 10 | TO5BIT(g) << 5 | TO5BIT(r))

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

Gameboy::Gameboy() : hram(highram + 0xe00), ioRam(highram + 0xf00) {
    saveFile = NULL;
    romFile = NULL;

    // private
    wroteToSramThisFrame = false;
    framesSinceAutosaveStarted = 0;

    externRam = NULL;
    saveModified = false;
    autosaveStarted = false;

    apu = new Gb_Apu();
    leftBuffer = new Mono_Buffer();
    rightBuffer = new Mono_Buffer();
    centerBuffer = new Mono_Buffer();

    ppu = new GameboyPPU(this);

    printer = new GameboyPrinter(this);

    cheatEngine = new CheatEngine(this);

    leftBuffer->set_sample_rate((long) SAMPLE_RATE);
    leftBuffer->bass_freq(461);
    leftBuffer->clock_rate(clockSpeed);

    rightBuffer->set_sample_rate((long) SAMPLE_RATE);
    rightBuffer->bass_freq(461);
    rightBuffer->clock_rate(clockSpeed);

    centerBuffer->set_sample_rate((long) SAMPLE_RATE);
    centerBuffer->bass_freq(461);
    centerBuffer->clock_rate(clockSpeed);

    apu->set_output(centerBuffer->center(), leftBuffer->center(), rightBuffer->center());

    ppu->probingForBorder = false;
}

Gameboy::~Gameboy() {
    unloadRom();

    delete apu;
    delete leftBuffer;
    delete rightBuffer;
    delete centerBuffer;
    delete ppu;
    delete printer;
    delete cheatEngine;
}

void Gameboy::init() {
    if(romFile == NULL) {
        return;
    }

    sgbMode = false;

    gameboyFrameCounter = 0;

    gameboyPaused = false;

    scanlineCounter = 456 * (doubleSpeed ? 2 : 1);
    phaseCounter = 456 * 153;
    timerCounter = 0;
    dividerCounter = 256;
    serialCounter = 0;

    cyclesToEvent = 0;
    extraCycles = 0;
    cyclesSinceVBlank = 0;
    cycleToSerialTransfer = -1;

    // Timer stuff
    periods[0] = clockSpeed / 4096;
    periods[1] = clockSpeed / 262144;
    periods[2] = clockSpeed / 65536;
    periods[3] = clockSpeed / 16384;
    timerPeriod = periods[0];

    memset(vram[0], 0, 0x2000);
    memset(vram[1], 0, 0x2000);

    memset(bgPaletteData, 0xff, 0x40);

    setDoubleSpeed(0);

    if(romFile->isGBS()) {
        resultantGBMode = 1; // GBC
        ppu->probingForBorder = false;
    } else {
        switch(gbcModeOption) {
            case 0: // GB
                initGBMode();
                break;
            case 1: // GBC if needed
                if(romFile->isCgbRequired()) {
                    initGBCMode();
                } else {
                    initGBMode();
                }

                break;
            case 2: // GBC
                if(romFile->isCgbSupported()) {
                    initGBCMode();
                } else {
                    initGBMode();
                }

                break;
        }

        if(romFile->isSgbEnhanced() && resultantGBMode != 2 && ppu->probingForBorder) {
            resultantGBMode = 2;
        } else {
            ppu->probingForBorder = false;
        }
    }

    biosOn = !ppu->probingForBorder && !romFile->isGBS() && ((biosMode == 1 && ((resultantGBMode != 1 && gbBiosLoaded) || gbcBiosLoaded)) || (biosMode == 2 && resultantGBMode != 1 && gbBiosLoaded) || (biosMode == 3 && gbcBiosLoaded));
    if(biosOn) {
        if(biosMode == 1) {
            gbMode = resultantGBMode != 1 && gbBiosLoaded ? GB : CGB;
        } else if(biosMode == 2) {
            gbMode = GB;
        } else if(biosMode == 3) {
            gbMode = CGB;
        }
    } else {
        initGameboyMode();
    }

    initMMU();
    initCPU();
    initSGB();
    initSND();
    ppu->initPPU();
    printer->initGbPrinter();

    refreshGFXPalette();

    if(romFile->isGBS()) {
        romFile->getGBS()->init(this);
    }
}

void Gameboy::initGBMode() {
    if(sgbModeOption != 0 && romFile->isSgbEnhanced()) {
        resultantGBMode = 2;
    } else {
        resultantGBMode = 0;
    }
}

void Gameboy::initGBCMode() {
    if(sgbModeOption == 2 && romFile->isSgbEnhanced()) {
        resultantGBMode = 2;
    } else {
        resultantGBMode = 1;
    }
}

void Gameboy::initSND() {
    soundCycles = 0;
    apu->reset(gbMode == GB ? Gb_Apu::mode_dmg : Gb_Apu::mode_cgb);
    leftBuffer->clear();
    rightBuffer->clear();
    centerBuffer->clear();

    writeIO(0x10, 0x80);
    writeIO(0x11, 0xBF);
    writeIO(0x12, 0xF3);
    writeIO(0x14, 0xBF);
    writeIO(0x16, 0x3F);
    writeIO(0x17, 0x00);
    writeIO(0x19, 0xBF);
    writeIO(0x1A, 0x7F);
    writeIO(0x1B, 0xFF);
    writeIO(0x1C, 0x9F);
    writeIO(0x1E, 0xBF);
    writeIO(0x20, 0xFF);
    writeIO(0x21, 0x00);
    writeIO(0x22, 0x00);
    writeIO(0x23, 0xBF);
    writeIO(0x26, 0xF0);
    writeIO(0x24, 0x77);
    writeIO(0x25, 0xF3);
}

// Called either from startup or when FF50 is written to.
void Gameboy::initGameboyMode() {
    gbRegs.af.b.l = 0xB0;
    gbRegs.bc.w = 0x0013;
    gbRegs.de.w = 0x00D8;
    gbRegs.hl.w = 0x014D;
    switch(resultantGBMode) {
        case 0: // GB
            gbRegs.af.b.h = 0x01;
            gbMode = GB;
            refreshGFXPalette();
            break;
        case 1: // GBC
            gbRegs.af.b.h = 0x11;
            if(gbaModeOption) {
                gbRegs.bc.b.h |= 1;
            }

            gbMode = CGB;
            break;
        case 2: // SGB
            sgbMode = true;
            gbRegs.af.b.h = 0x01;
            gbMode = GB;
            break;
    }

    memcpy(&g_gbRegs, &gbRegs, sizeof(Registers));
}

void Gameboy::gameboyCheckInput() {
    static int autoFireCounterA = 0;
    static int autoFireCounterB = 0;

    u8 buttonsPressed = 0xff;

    if(ppu->probingForBorder) {
        return;
    }

    if(inputKeyHeld(FUNC_KEY_UP)) {
        buttonsPressed &= (0xFF ^ GB_UP);
        if(!(ioRam[0x00] & 0x10)) {
            requestInterrupt(INT_JOYPAD);
        }
    }

    if(inputKeyHeld(FUNC_KEY_DOWN)) {
        buttonsPressed &= (0xFF ^ GB_DOWN);
        if(!(ioRam[0x00] & 0x10)) {
            requestInterrupt(INT_JOYPAD);
        }
    }

    if(inputKeyHeld(FUNC_KEY_LEFT)) {
        buttonsPressed &= (0xFF ^ GB_LEFT);
        if(!(ioRam[0x00] & 0x10)) {
            requestInterrupt(INT_JOYPAD);
        }
    }

    if(inputKeyHeld(FUNC_KEY_RIGHT)) {
        buttonsPressed &= (0xFF ^ GB_RIGHT);
        if(!(ioRam[0x00] & 0x10)) {
            requestInterrupt(INT_JOYPAD);
        }
    }

    if(inputKeyHeld(FUNC_KEY_A)) {
        buttonsPressed &= (0xFF ^ GB_A);
        if(!(ioRam[0x00] & 0x20)) {
            requestInterrupt(INT_JOYPAD);
        }
    }

    if(inputKeyHeld(FUNC_KEY_B)) {
        buttonsPressed &= (0xFF ^ GB_B);
        if(!(ioRam[0x00] & 0x20)) {
            requestInterrupt(INT_JOYPAD);
        }
    }

    if(inputKeyHeld(FUNC_KEY_START)) {
        buttonsPressed &= (0xFF ^ GB_START);
        if(!(ioRam[0x00] & 0x20)) {
            requestInterrupt(INT_JOYPAD);
        }
    }

    if(inputKeyHeld(FUNC_KEY_SELECT)) {
        buttonsPressed &= (0xFF ^ GB_SELECT);
        if(!(ioRam[0x00] & 0x20)) {
            requestInterrupt(INT_JOYPAD);
        }
    }

    if(inputKeyHeld(FUNC_KEY_AUTO_A)) {
        if(autoFireCounterA <= 0) {
            buttonsPressed &= (0xFF ^ GB_A);
            if(!(ioRam[0x00] & 0x20)) {
                requestInterrupt(INT_JOYPAD);
            }

            autoFireCounterA = 2;
        }

        autoFireCounterA--;
    }

    if(inputKeyHeld(FUNC_KEY_AUTO_B)) {
        if(autoFireCounterB <= 0) {
            buttonsPressed &= (0xFF ^ GB_B);
            if(!(ioRam[0x00] & 0x20)) {
                requestInterrupt(INT_JOYPAD);
            }

            autoFireCounterB = 2;
        }

        autoFireCounterB--;
    }

    controllers[0] = buttonsPressed;
}

// This is called 60 times per gameboy second, even if the lcd is off.
void Gameboy::updateVBlank() {
    gameboyFrameCounter++;

    if(!romFile->isGBS()) {
        if(ppu->probingForBorder) {
            if(gameboyFrameCounter >= 450) {
                // Give up on finding a sgb border.
                ppu->probingForBorder = false;
                ppu->sgbBorderLoaded = false;
                init();
            }

            return;
        }

        updateAutosave();

        if(cheatEngine->areCheatsEnabled()) {
            cheatEngine->applyGSCheats();
        }

        printer->updateGbPrinter();
    }
}

void Gameboy::pause() {
    if(!gameboyPaused) {
        gameboyPaused = true;
    }
}

void Gameboy::unpause() {
    if(gameboyPaused) {
        gameboyPaused = false;
    }
}

bool Gameboy::isGameboyPaused() {
    return gameboyPaused;
}

int Gameboy::runEmul() {
    if(gameboyPaused) {
        return RET_VBLANK;
    }

    emuRet = 0;
    memcpy(&g_gbRegs, &gbRegs, sizeof(Registers));

    if(cycleToSerialTransfer != -1) {
        setEventCycles(cycleToSerialTransfer);
    }

    while(true) {
        if(cyclesToEvent == MAX_WAIT_CYCLES) {
            cyclesToEvent = 1000;
        }

        int cycles = runOpcode(cyclesToEvent);
        cyclesToEvent = MAX_WAIT_CYCLES;

        bool opTriggeredInterrupt = cyclesToExecute == -1;

        cyclesSinceVBlank += cycles;
        updateTimers(cycles);
        updateSerial(cycles);
        updateSound(cycles);
        emuRet |= updateLCD(cycles);

        if(interruptTriggered) {
            /* Hack to fix Robocop 2 and LEGO Racers, possibly others. 
             * Interrupts can occur in the middle of an opcode. The result of 
             * this is that said opcode can read the resulting state - most 
             * importantly, it can read LY=144 before the vblank interrupt takes 
             * over. This is a decent approximation of that effect.
             * This has been known to break Megaman V boss intros, that's fixed 
             * by the "opTriggeredInterrupt" stuff.
             */
            if(!halt && !opTriggeredInterrupt) {
                extraCycles = runOpcode(extraCycles + 4);
            }

            if(interruptTriggered) {
                extraCycles += handleInterrupts(interruptTriggered);
                interruptTriggered = ioRam[0x0F] & ioRam[0xFF];
            }
        }

        if(emuRet) {
            memcpy(&gbRegs, &g_gbRegs, sizeof(Registers));
            return emuRet;
        }
    }
}

void Gameboy::refreshGFXPalette() {
    if(gbMode == GB && !sgbMode) {
        const unsigned short* palette = NULL;
        switch(gbColorizeMode) {
            case 0:
                palette = findPalette("GBC - Grayscale");
                break;
            case 1:
                // Don't set the game's palette until we're past the BIOS screen.
                if(!biosOn) {
                    palette = findPalette(romFile->getRomTitle().c_str());
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

        memcpy(bgPaletteData, palette, 4 * sizeof(u16));
        memcpy(sprPaletteData, palette + 4, 4 * sizeof(u16));
        memcpy(sprPaletteData + 4 * 8, palette + 8, 4 * sizeof(u16));

        ppu->refreshPPU();
    }
}

void Gameboy::checkLYC() {
    if(ioRam[0x44] == ioRam[0x45]) {
        ioRam[0x41] |= 4;
        if(ioRam[0x41] & 0x40) {
            requestInterrupt(INT_LCD);
        }
    } else {
        ioRam[0x41] &= ~4;
    }
}

inline int Gameboy::updateLCD(int cycles) {
    if(!(ioRam[0x40] & 0x80)) { // If LCD is off
        scanlineCounter = 456 * (doubleSpeed ? 2 : 1);
        ioRam[0x44] = 0;
        ioRam[0x41] &= 0xF8;

        // Normally timing is synchronized with gameboy's vblank. If the screen 
        // is off, this code kicks in. The "phaseCounter" is a counter until the 
        // ds should check for input and whatnot.
        phaseCounter -= cycles;
        if(phaseCounter <= 0) {
            phaseCounter += CYCLES_PER_FRAME << doubleSpeed;
            cyclesSinceVBlank = 0;
            // Though not technically vblank, this is a good time to check for 
            // input and whatnot.
            updateVBlank();
            return RET_VBLANK;
        }

        return 0;
    }

    scanlineCounter -= cycles;
    if(scanlineCounter > 0) {
        setEventCycles(scanlineCounter);
        return 0;
    }

    switch(ioRam[0x41] & 3) {
        case 0: // fall through to next case
        case 1:
            if(ioRam[0x44] == 0 && (ioRam[0x41] & 3) == 1) { // End of vblank
                ioRam[0x41]++; // Set mode 2
                scanlineCounter += 80 << doubleSpeed;
            } else {
                ioRam[0x44]++;
                checkLYC();

                if(ioRam[0x44] < 144 || ioRam[0x44] >= 153) { // Not in vblank
                    if(ioRam[0x41] & 0x20) {
                        requestInterrupt(INT_LCD);
                    }

                    if(ioRam[0x44] >= 153) {
                        // Don't change the mode. Scanline 0 is twice as
                        // long as normal - half of it identifies as being
                        // in the vblank period.
                        ioRam[0x44] = 0;
                        scanlineCounter += 456 << doubleSpeed;
                    } else { // End of hblank
                        ioRam[0x41] &= ~3;
                        ioRam[0x41] |= 2; // Set mode 2
                        if(ioRam[0x41] & 0x20) {
                            requestInterrupt(INT_LCD);
                        }

                        scanlineCounter += 80 << doubleSpeed;
                    }
                }

                checkLYC();

                if(ioRam[0x44] >= 144) { // In vblank
                    scanlineCounter += 456 << doubleSpeed;

                    if(ioRam[0x44] == 144) {// Beginning of vblank
                        ioRam[0x41] &= ~3;
                        ioRam[0x41] |= 1;   // Set mode 1

                        requestInterrupt(INT_VBLANK);
                        if(ioRam[0x41] & 0x10) {
                            requestInterrupt(INT_LCD);
                        }

                        cyclesSinceVBlank = scanlineCounter - (456 << doubleSpeed);
                        updateVBlank();
                        setEventCycles(scanlineCounter);
                        return RET_VBLANK;
                    }
                }
            }

            break;
        case 2:
            ioRam[0x41]++; // Set mode 3
            scanlineCounter += 172 << doubleSpeed;
            break;
        case 3:
            ioRam[0x41] &= ~3; // Set mode 0

            if(ioRam[0x41] & 0x8) {
                requestInterrupt(INT_LCD);
            }

            scanlineCounter += 204 << doubleSpeed;

            ppu->drawScanline(ioRam[0x44]);
            if(updateHBlankDMA()) {
                extraCycles += 8 << doubleSpeed;
            }

            break;
    }

    setEventCycles(scanlineCounter);
    return 0;
}

inline void Gameboy::updateTimers(int cycles) {
    if(ioRam[0x07] & 0x4) { // Timers enabled
        timerCounter -= cycles;
        while(timerCounter <= 0) {
            timerCounter += timerPeriod;
            ioRam[0x05]++;
            if(ioRam[0x05] == 0) {
                requestInterrupt(INT_TIMER);
                ioRam[0x05] = ioRam[0x06];
            }
        }

        // Set cycles until the timer will trigger an interrupt.
        // Reads from [0xff05] may be inaccurate.
        // However Castlevania and Alone in the Dark are extremely slow
        // if this is updated each time [0xff05] is changed.
        setEventCycles(timerCounter + timerPeriod * (255 - ioRam[0x05]));
    }

    dividerCounter -= cycles;
    while(dividerCounter <= 0) {
        dividerCounter += 256;
        ioRam[0x04]++;
    }
}

inline void Gameboy::updateSound(int cycles) {
    soundCycles += (cycles >> doubleSpeed);
    if(soundCycles >= CYCLES_PER_BUFFER) {
        apu->end_frame(CYCLES_PER_BUFFER);
        leftBuffer->end_frame(CYCLES_PER_BUFFER);
        rightBuffer->end_frame(CYCLES_PER_BUFFER);
        centerBuffer->end_frame(CYCLES_PER_BUFFER);
        soundCycles -= CYCLES_PER_BUFFER;

        if(soundEnabled && !gameboyPaused) {
            long leftCount = leftBuffer->read_samples((blip_sample_t*) audioGetLeftBuffer(), APU_BUFFER_SIZE);
            long rightCount = rightBuffer->read_samples((blip_sample_t*) audioGetRightBuffer(), APU_BUFFER_SIZE);
            long centerCount = centerBuffer->read_samples((blip_sample_t*) audioGetCenterBuffer(), APU_BUFFER_SIZE);
            audioPlay(leftCount, rightCount, centerCount);
        } else {
            leftBuffer->clear();
            rightBuffer->clear();
            centerBuffer->clear();
        }
    }
}

inline void Gameboy::updateSerial(int cycles) {
    // For external clock
    if(cycleToSerialTransfer != -1) {
        if(cyclesSinceVBlank < cycleToSerialTransfer) {
            setEventCycles(cycleToSerialTransfer - cyclesSinceVBlank);
        } else {
            cycleToSerialTransfer = -1;
            if((ioRam[0x02] & 0x81) == 0x80) {
                u8 tmp = ioRam[0x01];
                ioRam[0x01] = linkedGameboy->ioRam[0x01];
                linkedGameboy->ioRam[0x01] = tmp;
                emuRet |= RET_LINK;
                // Execution will be passed back to the other gameboy (the
                // internal clock gameboy).
            } else {
                linkedGameboy->ioRam[0x01] = 0xff;
            }

            if(ioRam[0x02] & 0x80) {
                requestInterrupt(INT_SERIAL);
                ioRam[0x02] &= ~0x80;
            }
        }
    }

    // For internal clock
    if(serialCounter > 0) {
        serialCounter -= cycles;
        if(serialCounter <= 0) {
            serialCounter = 0;
            if(linkedGameboy != NULL) {
                linkedGameboy->cycleToSerialTransfer = cyclesSinceVBlank;
                emuRet |= RET_LINK;
                // Execution will stop here, and this gameboy's SB will be
                // updated when the other gameboy runs to the appropriate
                // cycle.
            } else if(printerEnabled && romFile->getRomTitle().compare("ALLEY WAY") != 0) { // Alleyway breaks when the printer is enabled, so force disable it.
                ioRam[0x01] = printer->sendGbPrinterByte(ioRam[0x01]);
            } else {
                ioRam[0x01] = 0xFF;
            }

            requestInterrupt(INT_SERIAL);
            ioRam[0x02] &= ~0x80;
        } else {
            setEventCycles(serialCounter);
        }
    }
}


void Gameboy::requestInterrupt(int id) {
    ioRam[0x0F] |= id;
    interruptTriggered = (ioRam[0x0F] & ioRam[0xFF]);
    if(interruptTriggered) {
        cyclesToExecute = -1;
    }
}

void Gameboy::setDoubleSpeed(int val) {
    if(val == 0) {
        if(doubleSpeed) {
            scanlineCounter >>= 1;
        }

        doubleSpeed = 0;
        ioRam[0x4D] &= ~0x80;
    } else {
        if(!doubleSpeed) {
            scanlineCounter <<= 1;
        }

        doubleSpeed = 1;
        ioRam[0x4D] |= 0x80;
    }
}

bool Gameboy::loadRomFile(const char* filename) {
    if(romFile != NULL) {
        delete romFile;
        romFile = NULL;
    }

    if(filename == NULL) {
        return true;
    }

    romFile = new RomFile(this, filename);
    if(!romFile->isLoaded()) {
        delete romFile;
        romFile = NULL;
        return false;
    }

    // Load cheats
    if(romFile->isGBS()) {
        cheatEngine->loadCheats("");
    } else {
        cheatEngine->loadCheats((romFile->getFileName() + ".cht").c_str());
    }

    return true;
}

void Gameboy::unloadRom() {
    ppu->clearPPU();

    gameboySyncAutosave();

    if(saveFile != NULL) {
        fclose(saveFile);
        saveFile = NULL;
    }

    // unload previous save
    if(externRam != NULL) {
        free(externRam);
        externRam = NULL;
    }

    if(romFile != NULL) {
        delete romFile;
        romFile = NULL;
    }
}

bool Gameboy::isRomLoaded() {
    return romFile != NULL;
}

int Gameboy::loadSave() {
    if(romFile->getRamBanks() == 0) {
        return 0;
    }

    externRam = (u8*) malloc(romFile->getRamBanks() * 0x2000);

    if(romFile->isGBS()) {
        return 0;
    }

    // Now load the data.
    std::string savename = romFile->getFileName() + ".sav";
    saveFile = fopen(savename.c_str(), "r+b");

    int neededFileSize = romFile->getRamBanks() * 0x2000;
    if(romFile->getMBC() == MBC3 || romFile->getMBC() == HUC3 || romFile->getMBC() == TAMA5) {
        neededFileSize += sizeof(ClockStruct);
    }

    int fileSize = 0;
    if(saveFile) {
        struct stat s;
        fstat(fileno(saveFile), &s);
        fileSize = s.st_size;
    }

    if(!saveFile || fileSize < neededFileSize) {
        // Extend the size of the file, or create it
        if(!saveFile) {
            saveFile = fopen(savename.c_str(), "wb");
        }

        fseek(saveFile, neededFileSize - 1, SEEK_SET);
        fputc(0, saveFile);

        fclose(saveFile);
        saveFile = fopen(savename.c_str(), "r+b");
    }

    fread(externRam, 1, (size_t) (0x2000 * romFile->getRamBanks()), saveFile);

    switch(romFile->getMBC()) {
        case MBC3:
        case HUC3:
        case TAMA5:
            fread(&gbClock, 1, sizeof(gbClock), saveFile);
            break;
        default:
            break;
    }

    return 0;
}

int Gameboy::saveGame() {
    if(romFile->getRamBanks() == 0 || saveFile == NULL) {
        return 0;
    }

    fseek(saveFile, 0, SEEK_SET);

    fwrite(externRam, 1, (size_t) (0x2000 * romFile->getRamBanks()), saveFile);

    switch(romFile->getMBC()) {
        case MBC3:
        case HUC3:
        case TAMA5:
            fwrite(&gbClock, 1, sizeof(gbClock), saveFile);
            break;
        default:
            break;
    }

    return 0;
}

void Gameboy::gameboySyncAutosave() {
    if(!autosaveStarted) {
        return;
    }

    wroteToSramThisFrame = false;

    int numSectors = 0;
    // iterate over each 512-byte sector
    for(int i = 0; i < romFile->getRamBanks() * 0x2000 / 512; i++) {
        if(dirtySectors[i]) {
            dirtySectors[i] = false;
            numSectors++;

            fseek(saveFile, i * 512, SEEK_SET);
            fwrite(&externRam[i * 512], 1, 512, saveFile);
        }
    }

    systemPrintDebug("SAVE %d sectors\n", numSectors);

    framesSinceAutosaveStarted = 0;
    autosaveStarted = false;
}

void Gameboy::updateAutosave() {
    if(autosaveStarted) {
        framesSinceAutosaveStarted++;
    }

    // Executes when sram is written to for 120 consecutive frames, or
    // when a full frame has passed since sram was last written to.
    if(framesSinceAutosaveStarted >= 120 || (!saveModified && wroteToSramThisFrame)) {
        gameboySyncAutosave();
    }

    if(saveModified && autosaveEnabled) {
        wroteToSramThisFrame = true;
        autosaveStarted = true;
        saveModified = false;
    }
}

static const int STATE_VERSION = 10;

bool Gameboy::saveState(FILE* file) {
    if(!isRomLoaded() || file == NULL) {
        return false;
    }

    fwrite(&STATE_VERSION, 1, sizeof(int), file);
    fwrite(bgPaletteData, 1, sizeof(bgPaletteData), file);
    fwrite(sprPaletteData, 1, sizeof(sprPaletteData), file);
    fwrite(vram, 1, sizeof(vram), file);
    fwrite(wram, 1, sizeof(wram), file);
    fwrite(hram, 1, 0x200, file);
    fwrite(externRam, 1, (size_t) (0x2000 * romFile->getRamBanks()), file);

    fwrite(&gbRegs, 1, sizeof(gbRegs), file);
    fwrite(&halt, 1, sizeof(halt), file);
    fwrite(&ime, 1, sizeof(ime), file);
    fwrite(&doubleSpeed, 1, sizeof(doubleSpeed), file);
    fwrite(&biosOn, 1, sizeof(biosOn), file);
    fwrite(&gbMode, 1, sizeof(gbMode), file);
    fwrite(&romBank1Num, 1, sizeof(romBank1Num), file);
    fwrite(&ramBankNum, 1, sizeof(ramBankNum), file);
    fwrite(&wramBank, 1, sizeof(wramBank), file);
    fwrite(&vramBank, 1, sizeof(vramBank), file);
    fwrite(&memoryModel, 1, sizeof(memoryModel), file);
    fwrite(&gbClock, 1, sizeof(gbClock), file);
    fwrite(&scanlineCounter, 1, sizeof(scanlineCounter), file);
    fwrite(&timerCounter, 1, sizeof(timerCounter), file);
    fwrite(&phaseCounter, 1, sizeof(phaseCounter), file);
    fwrite(&dividerCounter, 1, sizeof(dividerCounter), file);
    fwrite(&serialCounter, 1, sizeof(serialCounter), file);
    fwrite(&ramEnabled, 1, sizeof(ramEnabled), file);
    fwrite(&romBank0Num, 1, sizeof(romBank0Num), file);
    fwrite(&haltBug, 1, sizeof(haltBug), file);

    bool gbBios = gbMode == GB;
    fwrite(&gbBios, 1, sizeof(gbBios), file);

    switch(romFile->getMBC()) {
        case HUC3:
            fwrite(&HuC3Mode, 1, sizeof(HuC3Mode), file);
            fwrite(&HuC3Value, 1, sizeof(HuC3Value), file);
            fwrite(&HuC3Shift, 1, sizeof(HuC3Shift), file);
            break;
        case MBC7:
            fwrite(&mbc7WriteEnable, 1, sizeof(mbc7WriteEnable), file);
            fwrite(&mbc7Idle, 1, sizeof(mbc7Idle), file);
            fwrite(&mbc7Cs, 1, sizeof(mbc7Cs), file);
            fwrite(&mbc7Sk, 1, sizeof(mbc7Sk), file);
            fwrite(&mbc7OpCode, 1, sizeof(mbc7OpCode), file);
            fwrite(&mbc7Addr, 1, sizeof(mbc7Addr), file);
            fwrite(&mbc7Cs, 1, sizeof(mbc7Cs), file);
            fwrite(&mbc7Count, 1, sizeof(mbc7Count), file);
            fwrite(&mbc7State, 1, sizeof(mbc7State), file);
            fwrite(&mbc7Buffer, 1, sizeof(mbc7Buffer), file);
            fwrite(&mbc7RA, 1, sizeof(mbc7RA), file);
            break;
        case MMM01:
            fwrite(&mmm01BankSelected, 1, sizeof(mmm01BankSelected), file);
            fwrite(&mmm01RomBaseBank, 1, sizeof(mmm01RomBaseBank), file);
            break;
        case CAMERA:
            fwrite(&cameraIO, 1, sizeof(cameraIO), file);
            break;
        case TAMA5:
            fwrite(&tama5CommandNumber, 1, sizeof(tama5CommandNumber), file);
            fwrite(&tama5RamByteSelect, 1, sizeof(tama5RamByteSelect), file);
            fwrite(&tama5Commands, 1, sizeof(tama5Commands), file);
            fwrite(&tama5RAM, 1, sizeof(tama5RAM), file);
        default:
            break;
    }

    fwrite(&sgbMode, 1, sizeof(sgbMode), file);
    if(sgbMode) {
        fwrite(&sgbPacketLength, 1, sizeof(sgbPacketLength), file);
        fwrite(&sgbPacketsTransferred, 1, sizeof(sgbPacketsTransferred), file);
        fwrite(&sgbPacketBit, 1, sizeof(sgbPacketBit), file);
        fwrite(&sgbCommand, 1, sizeof(sgbCommand), file);
        fwrite(&ppu->gfxMask, 1, sizeof(ppu->gfxMask), file);
        fwrite(sgbMap, 1, sizeof(sgbMap), file);
    }

    gb_apu_state_t apuState;
    apu->save_state(&apuState);
    fwrite(&apuState, 1, sizeof(apuState), file);

    return true;
}

bool Gameboy::loadState(FILE* file) {
    if(!isRomLoaded() || file == NULL) {
        return false;
    }

    int version;
    fread(&version, 1, sizeof(version), file);

    if(version == 0 || version > STATE_VERSION) {
        return false;
    }

    fread(bgPaletteData, 1, sizeof(bgPaletteData), file);
    fread(sprPaletteData, 1, sizeof(sprPaletteData), file);
    fread(vram, 1, sizeof(vram), file);
    fread(wram, 1, sizeof(wram), file);
    fread(hram, 1, 0x200, file);

    if(version <= 4 && romFile->getRamBanks() == 16) {
        // Value "0x04" for ram size wasn't interpreted correctly before
        fread(externRam, 1, 0x2000 * 4, file);
    } else {
        fread(externRam, 1, (size_t) (0x2000 * romFile->getRamBanks()), file);
    }

    fread(&gbRegs, 1, sizeof(gbRegs), file);
    fread(&halt, 1, sizeof(halt), file);
    fread(&ime, 1, sizeof(ime), file);
    fread(&doubleSpeed, 1, sizeof(doubleSpeed), file);
    fread(&biosOn, 1, sizeof(biosOn), file);

    if(version < 6) {
        fseek(file, 2, SEEK_CUR);
    }

    fread(&gbMode, 1, sizeof(gbMode), file);
    fread(&romBank1Num, 1, sizeof(romBank1Num), file);
    fread(&ramBankNum, 1, sizeof(ramBankNum), file);
    fread(&wramBank, 1, sizeof(wramBank), file);
    fread(&vramBank, 1, sizeof(vramBank), file);
    fread(&memoryModel, 1, sizeof(memoryModel), file);
    fread(&gbClock, 1, sizeof(gbClock), file);
    fread(&scanlineCounter, 1, sizeof(scanlineCounter), file);
    fread(&timerCounter, 1, sizeof(timerCounter), file);
    fread(&phaseCounter, 1, sizeof(phaseCounter), file);
    fread(&dividerCounter, 1, sizeof(dividerCounter), file);

    if(version >= 2) {
        fread(&serialCounter, 1, sizeof(serialCounter), file);
    } else {
        serialCounter = 0;
    }

    if(version >= 3) {
        fread(&ramEnabled, 1, sizeof(ramEnabled), file);
        if(version < 6) {
            fseek(file, 3, SEEK_CUR);
        }
    } else {
        ramEnabled = true;
    }

    if(version >= 6) {
        fread(&romBank0Num, 1, sizeof(romBank0Num), file);
    } else {
        romBank0Num = 0;
    }

    if(version >= 9) {
        fread(&haltBug, 1, sizeof(haltBug), file);
    } else {
        haltBug = false;
    }

    bool gbBios = false;
    if(version >= 10) {
        fread(&gbBios, 1, sizeof(gbBios), file);
    }

    // Some version 5 states will have the wrong BIOS flag set. Doubt many people have save states on the BIOS screen anyway.
    if(version == 5 || (gbBios && !gbBiosLoaded) || (!gbBios && !gbcBiosLoaded)) {
        biosOn = false;
    }

    /* MBC-specific values have been introduced in v3 */
    if(version >= 3) {
        switch(romFile->getMBC()) {
            case MBC3:
                if(version == 3) {
                    u8 rtcReg;
                    fread(&rtcReg, 1, sizeof(rtcReg), file);
                    if(rtcReg != 0) {
                        ramBankNum = rtcReg;
                    }
                }

                break;
            case MBC7:
                if(version >= 6) {
                    fread(&mbc7WriteEnable, 1, sizeof(mbc7WriteEnable), file);
                    fread(&mbc7Idle, 1, sizeof(mbc7Idle), file);
                    fread(&mbc7Cs, 1, sizeof(mbc7Cs), file);
                    fread(&mbc7Sk, 1, sizeof(mbc7Sk), file);
                    fread(&mbc7OpCode, 1, sizeof(mbc7OpCode), file);
                    fread(&mbc7Addr, 1, sizeof(mbc7Addr), file);
                    fread(&mbc7Cs, 1, sizeof(mbc7Cs), file);
                    fread(&mbc7Count, 1, sizeof(mbc7Count), file);
                    fread(&mbc7State, 1, sizeof(mbc7State), file);
                    fread(&mbc7Buffer, 1, sizeof(mbc7Buffer), file);
                    fread(&mbc7RA, 1, sizeof(mbc7RA), file);
                }

                break;
            case MMM01:
                if(version >= 6) {
                    fread(&mmm01BankSelected, 1, sizeof(mmm01BankSelected), file);
                    fread(&mmm01RomBaseBank, 1, sizeof(mmm01RomBaseBank), file);
                }

                break;
            case HUC3:
                fread(&HuC3Mode, 1, sizeof(HuC3Mode), file);
                fread(&HuC3Value, 1, sizeof(HuC3Value), file);
                fread(&HuC3Shift, 1, sizeof(HuC3Shift), file);
                break;
            case CAMERA:
                if(version >= 8) {
                    fread(&cameraIO, 1, sizeof(cameraIO), file);
                }

                break;
            case TAMA5:
                if(version >= 8) {
                    fread(&tama5CommandNumber, 1, sizeof(tama5CommandNumber), file);
                    fread(&tama5RamByteSelect, 1, sizeof(tama5RamByteSelect), file);
                    fread(&tama5Commands, 1, sizeof(tama5Commands), file);
                    fread(&tama5RAM, 1, sizeof(tama5RAM), file);
                }

                break;
            default:
                break;
        }
    }

    if(version >= 4) {
        fread(&sgbMode, 1, sizeof(sgbMode), file);
        if(sgbMode) {
            fread(&sgbPacketLength, 1, sizeof(sgbPacketLength), file);
            fread(&sgbPacketsTransferred, 1, sizeof(sgbPacketsTransferred), file);
            fread(&sgbPacketBit, 1, sizeof(sgbPacketBit), file);
            fread(&sgbCommand, 1, sizeof(sgbCommand), file);
            fread(&ppu->gfxMask, 1, sizeof(ppu->gfxMask), file);
            fread(sgbMap, 1, sizeof(sgbMap), file);
        }
    } else {
        sgbMode = false;
    }

    soundCycles = 0;
    apu->reset(gbMode == GB ? Gb_Apu::mode_dmg : Gb_Apu::mode_cgb);
    leftBuffer->clear();
    rightBuffer->clear();
    centerBuffer->clear();

    gb_apu_state_t apuState;
    if(version >= 7) {
        fread(&apuState, 1, sizeof(apuState), file);
    } else {
        apu->save_state(&apuState);
        for(int i = 0x10; i < 0x40; i++) {
            apuState.regs[i - 0x10] = ioRam[i];
        }
    }

    apu->load_state(apuState);

    timerPeriod = periods[ioRam[0x07] & 0x3];
    cyclesToEvent = 1;

    mapMemory();
    setDoubleSpeed(doubleSpeed);

    ppu->refreshPPU();

    return true;
}
