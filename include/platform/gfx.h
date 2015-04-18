#pragma once

#include <ctrcommon/types.hpp>

bool gfxInit();
void gfxCleanup();
void gfxToggleFastForward();
void gfxSetFastForward(bool fastforward);
void gfxLoadBorder(const char* filename);
void gfxLoadBorderBuffer(u8* imgData, u32 imgWidth, u32 imgHeight);
void gfxDrawPixel(int x, int y, u32 pixel);
void gfxDrawScreen();
void gfxFlush();
void gfxWaitForVBlank();
