#include <string.h>
#include <platform/system.h>

#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/gfx.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "ppu.h"
#include "sgb.h"
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

#define PALETTE_NUMBER (0x7)
#define VRAM_BANK (0x8)
#define SPRITE_NON_CGB_PALETTE_NUMBER (0x10)
#define FLIP_X (0x20)
#define FLIP_Y (0x40)
#define PRIORITY (0x80)

static const u8 depthOffset[8] = {
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
};

static const u8 BitReverseTable256[] = {
        0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
        0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
        0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
        0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
        0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
        0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
        0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
        0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
        0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
        0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
        0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
        0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
        0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
        0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
        0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
        0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

static const u32 BitStretchTable256[] = {
        0x0000, 0x0001, 0x0004, 0x0005, 0x0010, 0x0011, 0x0014, 0x0015, 0x0040, 0x0041, 0x0044, 0x0045, 0x0050, 0x0051, 0x0054, 0x0055,
        0x0100, 0x0101, 0x0104, 0x0105, 0x0110, 0x0111, 0x0114, 0x0115, 0x0140, 0x0141, 0x0144, 0x0145, 0x0150, 0x0151, 0x0154, 0x0155,
        0x0400, 0x0401, 0x0404, 0x0405, 0x0410, 0x0411, 0x0414, 0x0415, 0x0440, 0x0441, 0x0444, 0x0445, 0x0450, 0x0451, 0x0454, 0x0455,
        0x0500, 0x0501, 0x0504, 0x0505, 0x0510, 0x0511, 0x0514, 0x0515, 0x0540, 0x0541, 0x0544, 0x0545, 0x0550, 0x0551, 0x0554, 0x0555,
        0x1000, 0x1001, 0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015, 0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 0x1054, 0x1055,
        0x1100, 0x1101, 0x1104, 0x1105, 0x1110, 0x1111, 0x1114, 0x1115, 0x1140, 0x1141, 0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155,
        0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411, 0x1414, 0x1415, 0x1440, 0x1441, 0x1444, 0x1445, 0x1450, 0x1451, 0x1454, 0x1455,
        0x1500, 0x1501, 0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515, 0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551, 0x1554, 0x1555,
        0x4000, 0x4001, 0x4004, 0x4005, 0x4010, 0x4011, 0x4014, 0x4015, 0x4040, 0x4041, 0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055,
        0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 0x4114, 0x4115, 0x4140, 0x4141, 0x4144, 0x4145, 0x4150, 0x4151, 0x4154, 0x4155,
        0x4400, 0x4401, 0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415, 0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 0x4454, 0x4455,
        0x4500, 0x4501, 0x4504, 0x4505, 0x4510, 0x4511, 0x4514, 0x4515, 0x4540, 0x4541, 0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555,
        0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011, 0x5014, 0x5015, 0x5040, 0x5041, 0x5044, 0x5045, 0x5050, 0x5051, 0x5054, 0x5055,
        0x5100, 0x5101, 0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115, 0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 0x5154, 0x5155,
        0x5400, 0x5401, 0x5404, 0x5405, 0x5410, 0x5411, 0x5414, 0x5415, 0x5440, 0x5441, 0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455,
        0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 0x5514, 0x5515, 0x5540, 0x5541, 0x5544, 0x5545, 0x5550, 0x5551, 0x5554, 0x5555
};

static u16 RGBA5551ReverseTable[UINT16_MAX + 1];
static bool tablesInit = false;

static const int modeCycles[] = {
        204,
        456,
        80,
        172
};

void initTables() {
    if(!tablesInit) {
        for(u32 rgba5551 = 0; rgba5551 <= UINT16_MAX; rgba5551++) {
            RGBA5551ReverseTable[rgba5551] = (u16) ((rgba5551 & 0x1F) << 11 | ((rgba5551 >> 5) & 0x1F) << 6 | ((rgba5551 >> 10) & 0x1F) << 1 | 1);
        }

        tablesInit = true;
    }
}

PPU::PPU(Gameboy* gb) {
    this->gameboy = gb;
}

void PPU::reset() {
    initTables();

    this->lastScanlineCycle = 0;
    this->lastPhaseCycle = 0;
    this->halfSpeed = false;

    memset(this->vram[0], 0, 0x2000);
    memset(this->vram[1], 0, 0x2000);
    memset(this->oam, 0, 0xA0);

    memset(this->bgPaletteData, 0xFF, sizeof(this->bgPaletteData));
    memset(this->sprPaletteData, 0xFF, sizeof(this->sprPaletteData));

    this->gameboy->mmu->writeIO(LCDC, 0x91);
    this->gameboy->mmu->writeIO(BGP, 0xFC);
    this->gameboy->mmu->writeIO(OBP0, 0xFF);
    this->gameboy->mmu->writeIO(OBP1, 0xFF);
    this->gameboy->mmu->writeIO(HDMA5, 0xFF);

    this->bgPaletteData[0] = TOCGB(255, 255, 255);
    this->bgPaletteData[1] = TOCGB(192, 192, 192);
    this->bgPaletteData[2] = TOCGB(94, 94, 94);
    this->bgPaletteData[3] = TOCGB(0, 0, 0);
    this->sprPaletteData[0] = TOCGB(255, 255, 255);
    this->sprPaletteData[1] = TOCGB(192, 192, 192);
    this->sprPaletteData[2] = TOCGB(94, 94, 94);
    this->sprPaletteData[3] = TOCGB(0, 0, 0);
    this->sprPaletteData[4] = TOCGB(255, 255, 255);
    this->sprPaletteData[5] = TOCGB(192, 192, 192);
    this->sprPaletteData[6] = TOCGB(94, 94, 94);
    this->sprPaletteData[7] = TOCGB(0, 0, 0);

    this->refreshGBPalette();

    this->mapBanks();

    this->gameboy->mmu->mapIOReadFunc(BCPD, [this](u16 addr) -> u8 {
        return ((u8*) this->bgPaletteData)[this->gameboy->mmu->readIO(BCPS) & 0x3F];
    });

    this->gameboy->mmu->mapIOReadFunc(OCPD, [this](u16 addr) -> u8 {
        return ((u8*) this->sprPaletteData)[this->gameboy->mmu->readIO(OCPS) & 0x3F];
    });

    this->gameboy->mmu->mapIOWriteFunc(LCDC, [this](u16 addr, u8 val) -> void {
        if((this->gameboy->mmu->readIO(LCDC) & 0x80) && !(val & 0x80)) {
            gfxClearScreenBuffer(0xFFFF);
        }

        this->gameboy->mmu->writeIO(LCDC, val);
        if(!(val & 0x80)) {
            this->gameboy->mmu->writeIO(LY, 0);
            this->gameboy->mmu->writeIO(STAT, (u8) (this->gameboy->mmu->readIO(STAT) & ~3)); // Set video mode 0
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(STAT, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(STAT, (u8) ((this->gameboy->mmu->readIO(STAT) & 0x7) | (val & 0xF8)));
    });

    this->gameboy->mmu->mapIOWriteFunc(LY, [this](u16 addr, u8 val) -> void {
    });

    this->gameboy->mmu->mapIOWriteFunc(LYC, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(LYC, val);
        this->checkLYC();
    });

    this->gameboy->mmu->mapIOWriteFunc(DMA, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(DMA, val);

        u16 src = val << 8;
        for(u8 i = 0; i < 0xA0; i++) {
            this->oam[i] = this->gameboy->mmu->read(src + i);
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(VBK, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            this->gameboy->mmu->writeIO(VBK, (u8) (val & 1));
            this->mapBanks();
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(BCPD, [this](u16 addr, u8 val) -> void {
        u8 bcps = this->gameboy->mmu->readIO(BCPS);
        ((u8*) this->bgPaletteData)[bcps & 0x3F] = val;
        if(bcps & 0x80) {
            this->gameboy->mmu->writeIO(BCPS, (u8) (((bcps + 1) & 0x3F) | (bcps & 0x80)));
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(OCPD, [this](u16 addr, u8 val) -> void {
        u8 ocps = this->gameboy->mmu->readIO(OCPS);
        ((u8*) this->sprPaletteData)[ocps & 0x3F] = val;
        if(ocps & 0x80) {
            this->gameboy->mmu->writeIO(OCPS, (u8) (((ocps + 1) & 0x3F) | (ocps & 0x80)));
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(HDMA5, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            if((this->gameboy->mmu->readIO(HDMA5) & 0x80) == 0) {
                if((val & 0x80) == 0) {
                    this->gameboy->mmu->writeIO(HDMA5, (u8) (val | 0x80));
                }
            } else {
                if(((val >> 7) & 1) == 0) {
                    u8 bank = this->gameboy->mmu->readIO(VBK);
                    u16 src = (u16) ((this->gameboy->mmu->readIO(HDMA2) | (this->gameboy->mmu->readIO(HDMA1) << 8)) & 0xFFF0);
                    u16 dst = (u16) ((this->gameboy->mmu->readIO(HDMA4) | (this->gameboy->mmu->readIO(HDMA3) << 8)) & 0x1FF0);
                    u8 length = (u8) ((val & 0x7F) + 1);
                    for(u8 i = 0; i < length; i++) {
                        for(u8 j = 0; j < 0x10; j++) {
                            this->vram[bank][dst++] = this->gameboy->mmu->read(src++);
                        }

                        dst &= 0x1FF0;
                    }

                    this->gameboy->mmu->writeIO(HDMA1, (u8) ((src >> 8) & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA2, (u8) (src & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA3, (u8) ((dst >> 8) & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA4, (u8) (dst & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA5, 0xFF);

                    this->gameboy->cpu->advanceCycles((u64) ((length * 8) << this->halfSpeed));
                } else {
                    this->gameboy->mmu->writeIO(HDMA5, (u8) (val & 0x7F));
                }
            }
        }
    });
}

void PPU::loadState(FILE* file, int version) {
    fread(&this->lastScanlineCycle, 1, sizeof(this->lastScanlineCycle), file);
    fread(&this->lastPhaseCycle, 1, sizeof(this->lastPhaseCycle), file);
    fread(&this->halfSpeed, 1, sizeof(this->halfSpeed), file);
    fread(this->vram, 1, sizeof(this->vram), file);
    fread(this->oam, 1, sizeof(this->oam), file);
    fread(this->bgPaletteData, 1, sizeof(this->bgPaletteData), file);
    fread(this->sprPaletteData, 1, sizeof(this->sprPaletteData), file);

    this->mapBanks();
}

void PPU::saveState(FILE* file) {
    fwrite(&this->lastScanlineCycle, 1, sizeof(this->lastScanlineCycle), file);
    fwrite(&this->lastPhaseCycle, 1, sizeof(this->lastPhaseCycle), file);
    fwrite(&this->halfSpeed, 1, sizeof(this->halfSpeed), file);
    fwrite(this->vram, 1, sizeof(this->vram), file);
    fwrite(this->oam, 1, sizeof(this->oam), file);
    fwrite(this->bgPaletteData, 1, sizeof(this->bgPaletteData), file);
    fwrite(this->sprPaletteData, 1, sizeof(this->sprPaletteData), file);
}

void PPU::checkLYC() {
    u8 stat = this->gameboy->mmu->readIO(STAT);
    if(this->gameboy->mmu->readIO(LY) == this->gameboy->mmu->readIO(LYC)) {
        this->gameboy->mmu->writeIO(STAT, (u8) (stat | 4));
        if(stat & 0x40) {
            this->gameboy->cpu->requestInterrupt(INT_LCD);
        }
    } else {
        this->gameboy->mmu->writeIO(STAT, (u8) (stat & ~4));
    }
}

int PPU::update() {
    int ret = 0;

    if((this->gameboy->mmu->readIO(LCDC) & 0x80) == 0) { // If LCD is off
        this->lastScanlineCycle = this->gameboy->cpu->getCycle();

        this->gameboy->mmu->writeIO(LY, 0);
        this->gameboy->mmu->writeIO(STAT, (u8) (this->gameboy->mmu->readIO(STAT) & 0xF8));

        // Normally timing is synchronized with gameboy's vblank. If the screen
        // is off, this code kicks in. The "lastPhaseCycle" is the last cycle
        // checked for input and whatnot.
        while(this->gameboy->cpu->getCycle() >= this->lastPhaseCycle + (CYCLES_PER_FRAME << this->halfSpeed)) {
            this->lastPhaseCycle += CYCLES_PER_FRAME << this->halfSpeed;
            // Though not technically vblank, this is a good time to check for
            // input and whatnot.
            ret |= RET_VBLANK;
        }

        this->gameboy->cpu->setEventCycle(this->lastPhaseCycle + (CYCLES_PER_FRAME << this->halfSpeed));
    } else {
        this->lastPhaseCycle = this->gameboy->cpu->getCycle();

        while(this->gameboy->cpu->getCycle() >= this->lastScanlineCycle + (modeCycles[this->gameboy->mmu->readIO(STAT) & 3] << this->halfSpeed)) {
            this->lastScanlineCycle += modeCycles[this->gameboy->mmu->readIO(STAT) & 3] << this->halfSpeed;

            switch(this->gameboy->mmu->readIO(STAT) & 3) {
                case 0: // fall through to next case
                case 1:
                    if(this->gameboy->mmu->readIO(LY) == 0 && (this->gameboy->mmu->readIO(STAT) & 3) == 1) { // End of vblank
                        this->gameboy->mmu->writeIO(STAT, (u8) (this->gameboy->mmu->readIO(STAT) + 1)); // Set mode 2
                    } else {
                        this->gameboy->mmu->writeIO(LY, (u8) (this->gameboy->mmu->readIO(LY) + 1));
                        this->checkLYC();

                        if(this->gameboy->mmu->readIO(LY) < 144 || this->gameboy->mmu->readIO(LY) >= 153) { // Not in vblank
                            if(this->gameboy->mmu->readIO(STAT) & 0x20) {
                                this->gameboy->cpu->requestInterrupt(INT_LCD);
                            }

                            if(this->gameboy->mmu->readIO(LY) >= 153) {
                                // Don't change the mode. Scanline 0 is twice as
                                // long as normal - half of it identifies as being
                                // in the vblank period.
                                this->gameboy->mmu->writeIO(LY, 0);
                                this->checkLYC();
                            } else { // End of hblank
                                this->gameboy->mmu->writeIO(STAT, (u8) ((this->gameboy->mmu->readIO(STAT) & ~3) | 2)); // Set mode 2
                                if(this->gameboy->mmu->readIO(STAT) & 0x20) {
                                    this->gameboy->cpu->requestInterrupt(INT_LCD);
                                }
                            }
                        } else if(this->gameboy->mmu->readIO(LY) == 144) {// Beginning of vblank
                            this->gameboy->mmu->writeIO(STAT, (u8) ((this->gameboy->mmu->readIO(STAT) & ~3) | 1)); // Set mode 1

                            this->gameboy->cpu->requestInterrupt(INT_VBLANK);
                            if(this->gameboy->mmu->readIO(STAT) & 0x10) {
                                this->gameboy->cpu->requestInterrupt(INT_LCD);
                            }

                            ret |= RET_VBLANK;
                        }
                    }

                    break;
                case 2:
                    this->gameboy->mmu->writeIO(STAT, (u8) (this->gameboy->mmu->readIO(STAT) + 1)); // Set mode 3
                    break;
                case 3: {
                    this->gameboy->mmu->writeIO(STAT, (u8) (this->gameboy->mmu->readIO(STAT) & ~3)); // Set mode 0

                    if(this->gameboy->mmu->readIO(STAT) & 0x8) {
                        this->gameboy->cpu->requestInterrupt(INT_LCD);
                    }

                    if((!gfxGetFastForward() || fastForwardCounter >= fastForwardFrameSkip) && !this->gameboy->romFile->isGBS()) {
                        this->drawScanline(this->gameboy->mmu->readIO(LY));
                    }

                    u8 hdma5 = this->gameboy->mmu->readIO(HDMA5);
                    if((hdma5 & 0x80) == 0) {
                        u8 bank = this->gameboy->mmu->readIO(VBK);
                        u16 src = (u16) ((this->gameboy->mmu->readIO(HDMA2) | (this->gameboy->mmu->readIO(HDMA1) << 8)) & 0xFFF0);
                        u16 dst = (u16) ((this->gameboy->mmu->readIO(HDMA4) | (this->gameboy->mmu->readIO(HDMA3) << 8)) & 0x1FF0);
                        for(u8 i = 0; i < 0x10; i++) {
                            this->vram[bank][dst++] = this->gameboy->mmu->read(src++);
                        }

                        dst &= 0x1FF0;

                        this->gameboy->mmu->writeIO(HDMA1, (u8) ((src >> 8) & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA2, (u8) (src & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA3, (u8) ((dst >> 8) & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA4, (u8) (dst & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA5, (u8) (hdma5 - 1));

                        this->gameboy->cpu->advanceCycles((u64) (8 << this->halfSpeed));
                    }

                    break;
                }
                default:
                    break;
            }
        }

        this->gameboy->cpu->setEventCycle(this->lastScanlineCycle + (modeCycles[this->gameboy->mmu->readIO(STAT) & 3] << this->halfSpeed));
    }

    return ret;
}

void PPU::mapBanks() {
    u8 bank = this->gameboy->mmu->readIO(VBK);
    this->gameboy->mmu->mapBank(0x8, this->vram[bank] + 0x0000);
    this->gameboy->mmu->mapBank(0x9, this->vram[bank] + 0x1000);
}

void PPU::setHalfSpeed(bool halfSpeed) {
    if(!this->halfSpeed && halfSpeed) {
        this->lastScanlineCycle -= this->gameboy->cpu->getCycle() - this->lastScanlineCycle;
        this->lastPhaseCycle -= this->gameboy->cpu->getCycle() - this->lastPhaseCycle;
    } else if(this->halfSpeed && !halfSpeed) {
        this->lastScanlineCycle += (this->gameboy->cpu->getCycle() - this->lastScanlineCycle) / 2;
        this->lastPhaseCycle += (this->gameboy->cpu->getCycle() - this->lastPhaseCycle) / 2;
    }

    this->halfSpeed = halfSpeed;
}

void PPU::refreshGBPalette() {
    if(this->gameboy->isRomLoaded() && this->gameboy->gbMode == MODE_GB) {
        const u16* palette = NULL;
        switch(gbColorizeMode) {
            case 0:
                palette = findPalette("GBC - Grayscale");
                break;
            case 1:
                // Don't set the game's palette until we're past the BIOS screen.
                if(!this->gameboy->biosOn) {
                    palette = findPalette(this->gameboy->romFile->getRomTitle().c_str());
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

        memcpy(this->bgPaletteData, palette, 4 * sizeof(u16));
        memcpy(this->sprPaletteData, palette + 4, 4 * sizeof(u16));
        memcpy(this->sprPaletteData + 4 * 4, palette + 8, 4 * sizeof(u16));
    }
}

void PPU::transferTiles(u8* dest) {
    u8 lcdc = this->gameboy->mmu->readIO(LCDC);

    u32 map = (u32) (0x1800 + ((lcdc >> 3) & 1) * 0x400);
    u32 index = 0;
    for(u32 y = 0; y < 18; y++) {
        for(u32 x = 0; x < 20; x++) {
            if(index == 0x1000) {
                return;
            }

            u8 tile = this->vram[0][map + y * 32 + x];
            if(lcdc & 0x10) {
                memcpy(dest + index, &this->vram[0][tile * 0x10], 0x10);
            } else {
                memcpy(dest + index, &this->vram[0][0x1000 + (s8) tile * 0x10], 0x10);
            }

            index += 0x10;
        }
    }
}

void PPU::drawScanline(u32 scanline) {
    switch(this->gameboy->sgb->getGfxMask()) {
        case 0: {
            u16* lineBuffer = gfxGetLineBuffer(scanline);
            u8 depthBuffer[256] = {0};

            this->drawBackground(lineBuffer, depthBuffer, scanline);
            this->drawWindow(lineBuffer, depthBuffer, scanline);
            this->drawSprites(lineBuffer, depthBuffer, scanline);
            break;
        }
        case 2:
            gfxClearLineBuffer(scanline, 0x0000);
            break;
        case 3:
            gfxClearLineBuffer(scanline, RGBA5551ReverseTable[this->bgPaletteData[this->gameboy->gbMode != MODE_CGB ? this->gameboy->mmu->readIO(BGP) & 3 : 0]]);
            break;
        default:
            break;
    }
}

void PPU::drawBackground(u16* lineBuffer, u8* depthBuffer, u32 scanline) {
    u8 lcdc = this->gameboy->mmu->readIO(LCDC);
    if(this->gameboy->gbMode == MODE_CGB || (lcdc & 0x01) != 0) { // Background enabled
        u8 scx = this->gameboy->mmu->readIO(SCX);
        u8 scy = this->gameboy->mmu->readIO(SCY);
        u8 wx = this->gameboy->mmu->readIO(WX);
        u8 wy = this->gameboy->mmu->readIO(WY);
        u8 bgp = this->gameboy->mmu->readIO(BGP);

        u8* sgbMap = this->gameboy->sgb->getGfxMap();

        bool tileSigned = (lcdc & 0x10) == 0;
        bool losePriority = this->gameboy->gbMode == MODE_CGB && (lcdc & 0x01) == 0;

        // The y position (measured in tiles)
        u32 tileY = ((scanline + scy) & 0xFF) / 8;
        u32 basePixelY = (scanline + scy) & 0x07;

        // Tile Map address plus row offset
        u32 bgMapAddr = 0x1800 + ((lcdc >> 3) & 1) * 0x400 + (tileY * 32);

        // Number of tiles to draw in a row
        u32 numTilesX = 20;
        if(wy <= scanline && wy < 144 && wx >= 7 && wx < 167 && (lcdc & 0x20) != 0) {
            numTilesX = (u32) (wx / 8);
        }

        // Tiles to draw
        u32 startTile = (u32) (scx / 8);
        u32 endTile = (startTile + numTilesX + 1) & 31;

        // Calculate lineBuffer Start, negatives treated as unsigned for speed up
        u8 writeX = (u8) (-(scx & 0x07));
        for(u32 tile = startTile; tile != endTile; tile = (tile + 1) & 31) {
            // The address, from the beginning of VRAM, of the tile's mapping
            u32 mapAddr = bgMapAddr + tile;

            u32 pixelY = basePixelY;
            u8 depth = 1;
            u32 paletteId = 0;

            u32 vRamB1 = 0;
            u32 vRamB2 = 0;

            // This is the tile id.
            u32 tileNum = this->vram[0][mapAddr];
            if(tileSigned) {
                tileNum = ((s8) tileNum) + 256;
            }

            // Setup Tile Info
            if(this->gameboy->gbMode == MODE_CGB) {
                u32 flag = this->vram[1][mapAddr];
                paletteId = flag & PALETTE_NUMBER;

                if(flag & FLIP_Y) {
                    pixelY = 7 - pixelY;
                }

                // Read bytes of tile line
                u32 bank = (u32) ((flag & VRAM_BANK) != 0);
                vRamB1 = this->vram[bank][(tileNum << 4) + (pixelY << 1)];
                vRamB2 = this->vram[bank][(tileNum << 4) + (pixelY << 1) + 1];

                // Reverse their bits if flipX set
                if(!(flag & FLIP_X)) {
                    vRamB1 = BitReverseTable256[vRamB1];
                    vRamB2 = BitReverseTable256[vRamB2];
                }

                // Setup depth based on priority
                if(losePriority) {
                    depth = 0;
                } else if(flag & PRIORITY) {
                    depth = 3;
                }
            } else {
                // Read bytes of tile line
                vRamB1 = BitReverseTable256[this->vram[0][(tileNum << 4) + (pixelY << 1)]];
                vRamB2 = BitReverseTable256[this->vram[0][(tileNum << 4) + (pixelY << 1) + 1]];
            }

            // Mux the bits to for more logical pixels
            u32 pxData = BitStretchTable256[vRamB1] | (BitStretchTable256[vRamB2] << 1);

            u8* subSgbMap = &sgbMap[scanline / 8 * 20];
            for(u32 x = 0; x < 16; x += 2, writeX++) {
                if(writeX >= 160) {
                    continue;
                }

                u32 colorId = (pxData >> x) & 0x03;
                u32 subPaletteId = this->gameboy->gbMode == MODE_SGB ? subSgbMap[writeX >> 3] : paletteId;
                u8 paletteIndex = this->gameboy->gbMode != MODE_CGB ? (u8) ((bgp >> (colorId * 2)) & 3) : (u8) colorId;

                // Draw pixel
                depthBuffer[writeX] = depth - depthOffset[losePriority * 4 + colorId];
                lineBuffer[writeX] = RGBA5551ReverseTable[this->bgPaletteData[subPaletteId * 4 + paletteIndex]];
            }
        }
    }
}

void PPU::drawWindow(u16* lineBuffer, u8* depthBuffer, u32 scanline) {
    u8 lcdc = this->gameboy->mmu->readIO(LCDC);
    u8 wx = this->gameboy->mmu->readIO(WX);
    u8 wy = this->gameboy->mmu->readIO(WY);
    if(wy <= scanline && wy < 144 && wx >= 7 && wx < 167 && (lcdc & 0x20) != 0) { // Window enabled
        u8 bgp = this->gameboy->mmu->readIO(BGP);

        u8* sgbMap = this->gameboy->sgb->getGfxMap();

        bool tileSigned = (lcdc & 0x10) == 0;
        bool losePriority = this->gameboy->gbMode == MODE_CGB && (lcdc & 0x01) == 0;

        // The y position (measured in tiles)
        u32 tileY = (scanline - wy) / 8;
        u32 basePixelY = (scanline - wy) & 0x07;

        // Tile Map address plus row offset
        u32 winMapAddr = 0x1800 + ((lcdc >> 6) & 1) * 0x400 + (tileY * 32);

        // Tiles to draw
        u32 endTile = (u32) (21 - (wx - 7) / 8);

        // Calculate lineBuffer Start, negatives treated as unsigned for speed up
        u32 writeX = (u32) (wx - 7);
        for(u32 tile = 0; tile < endTile; tile++) {
            // The address, from the beginning of VRAM, of the tile's mapping
            u32 mapAddr = winMapAddr + tile;

            u32 pixelY = basePixelY;
            u8 depth = 1;
            u32 paletteId = 0;

            u32 vRamB1 = 0;
            u32 vRamB2 = 0;

            // This is the tile id.
            u32 tileNum = this->vram[0][mapAddr];
            if(tileSigned) {
                tileNum = ((s8) tileNum) + 256;
            }

            // Setup Tile Info
            if(this->gameboy->gbMode == MODE_CGB) {
                u32 flag = this->vram[1][mapAddr];
                paletteId = flag & PALETTE_NUMBER;

                if(flag & FLIP_Y) {
                    pixelY = 7 - pixelY;
                }

                // Read bytes of tile line
                u32 bank = (u32) ((flag & VRAM_BANK) != 0);
                vRamB1 = this->vram[bank][(tileNum << 4) + (pixelY << 1)];
                vRamB2 = this->vram[bank][(tileNum << 4) + (pixelY << 1) + 1];

                // Reverse their bits if flipX set
                if(!(flag & FLIP_X)) {
                    vRamB1 = BitReverseTable256[vRamB1];
                    vRamB2 = BitReverseTable256[vRamB2];
                }

                // Setup depth based on priority
                if(losePriority) {
                    depth = 0;
                } else if(flag & PRIORITY) {
                    depth = 3;
                }
            } else {
                // Read bytes of tile line
                vRamB1 = BitReverseTable256[this->vram[0][(tileNum << 4) + (pixelY << 1)]];
                vRamB2 = BitReverseTable256[this->vram[0][(tileNum << 4) + (pixelY << 1) + 1]];
            }

            // Mux the bits to for more logical pixels
            u32 pxData = BitStretchTable256[vRamB1] | (BitStretchTable256[vRamB2] << 1);

            u8* subSgbMap = &sgbMap[scanline / 8 * 20];
            for(u32 x = 0; x < 16; x += 2, writeX++) {
                if(writeX >= 160) {
                    continue;
                }

                u32 colorId = (pxData >> x) & 0x03;
                u32 subPaletteId = this->gameboy->gbMode == MODE_SGB ? subSgbMap[writeX >> 3] : paletteId;
                u8 paletteIndex = this->gameboy->gbMode != MODE_CGB ? (u8) ((bgp >> (colorId * 2)) & 3) : (u8) colorId;

                // Draw pixel
                depthBuffer[writeX] = depth - depthOffset[losePriority * 4 + colorId];
                lineBuffer[writeX] = RGBA5551ReverseTable[this->bgPaletteData[subPaletteId * 4 + paletteIndex]];
            }
        }
    }
}

void PPU::drawSprites(u16* lineBuffer, u8* depthBuffer, u32 scanline) {
    u8 lcdc = this->gameboy->mmu->readIO(LCDC);
    if(lcdc & 0x2) { // Sprites enabled
        u8* sgbMap = this->gameboy->sgb->getGfxMap();

        for(u32 sprite = 39; (int) sprite >= 0; sprite--) {
            // The sprite's number, times 4 (each uses 4 bytes)
            u32 spriteOffset = sprite * 4;

            u32 y = this->oam[spriteOffset];
            u32 height = (lcdc & 0x4) ? 16 : 8;

            // Clip Sprite to or bottom
            if(scanline + 16 < y || scanline + 16 >= y + height) {
                continue;
            }

            // Setup Tile Info
            u32 tileNum = this->oam[spriteOffset + 2];
            u32 flag = this->oam[spriteOffset + 3];
            u32 bank = 0;
            u32 paletteId = 0;
            if(this->gameboy->gbMode == MODE_CGB) {
                bank = (flag & VRAM_BANK) >> 3;
                paletteId = flag & PALETTE_NUMBER;
            } else {
                paletteId = (flag & SPRITE_NON_CGB_PALETTE_NUMBER) >> 2;
            }

            u8 obp = this->gameboy->mmu->readIO((u16) (OBP0 + paletteId / 4));

            // Select tile base on tile Y offset
            if(height == 16) {
                tileNum &= ~1;
                if(scanline - y + 16 >= 8) {
                    tileNum++;
                }
            }

            // This is the tile's Y position to be read (0-7)
            u32 pixelY = (scanline - y + 16) & 0x07;
            if(flag & FLIP_Y) {
                pixelY = 7 - pixelY;
                if(height == 16) {
                    tileNum = tileNum ^ 1;
                }
            }

            // Setup depth based on priority
            u8 depth = 2;
            if(flag & PRIORITY) {
                depth = 0;
            }

            // Read bytes of tile line
            u32 vRamB1 = this->vram[bank][(tileNum << 4) + (pixelY << 1)];
            u32 vRamB2 = this->vram[bank][(tileNum << 4) + (pixelY << 1) + 1];

            // Reverse their bits if flipX set
            if(!(flag & FLIP_X)) {
                vRamB1 = BitReverseTable256[vRamB1];
                vRamB2 = BitReverseTable256[vRamB2];
            }

            // Mux the bits to for more logical pixels
            u32 pxData = BitStretchTable256[vRamB1] | (BitStretchTable256[vRamB2] << 1);

            // Calculate where to start to draw, negatives treated as unsigned for speed up
            u32 writeX = (u32) ((s32) (this->oam[spriteOffset + 1] - 8));

            u8* subSgbMap = &sgbMap[scanline / 8 * 20];
            for(u32 x = 0; x < 16; x += 2, writeX++) {
                if(writeX >= 160) {
                    continue;
                }

                u32 colorId = (pxData >> x) & 0x03;

                // Draw pixel if not transparent or above depth buffer
                if(colorId != 0 && depth >= depthBuffer[writeX]) {
                    u32 subPaletteId = this->gameboy->gbMode == MODE_SGB ? paletteId + subSgbMap[writeX >> 3] : paletteId;
                    u8 paletteIndex = this->gameboy->gbMode != MODE_CGB ? (u8) ((obp >> (colorId * 2)) & 3) : (u8) colorId;

                    depthBuffer[writeX] = depth;
                    lineBuffer[writeX] = RGBA5551ReverseTable[this->sprPaletteData[subPaletteId * 4 + paletteIndex]];
                }
            }
        }
    }
}
