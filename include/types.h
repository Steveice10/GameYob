#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#include <string>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

inline u32 RGB555ComponentsToRGB8888(u8 r5, u8 g5, u8 b5) {
    u8 r8 = ((r5 << 3) | (r5 >> 2));
    u8 g8 = ((g5 << 3) | (g5 >> 2));
    u8 b8 = ((b5 << 3) | (b5 >> 2));

    return (u32) (r8 << 24 | g8 << 16 | b8 << 8 | 0xFF);
}

inline u32 RGB555ToRGB8888(u16 rgb555) {
    u8 r5 = (u8) (rgb555 & 0x1F);
    u8 g5 = (u8) ((rgb555 >> 5) & 0x1F);
    u8 b5 = (u8) ((rgb555 >> 10) & 0x1F);

    return RGB555ComponentsToRGB8888(r5, g5, b5);
}