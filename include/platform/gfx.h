#pragma once

#include "types.h"

#define RGBA32(r, g, b) ((u32) ((r) << 24 | (g) << 16 | (b) << 8 | 0xFF))

bool gfxInit();
void gfxCleanup();
bool gfxGetFastForward();
void gfxSetFastForward(bool fastforward);
void gfxToggleFastForward();
void gfxLoadBorder(u8* imgData, u32 imgWidth, u32 imgHeight);
u32* gfxGetLineBuffer(int line);
void gfxClearScreenBuffer(u8 r, u8 g, u8 b);
void gfxDrawScreen();
void gfxWaitForVBlank();
