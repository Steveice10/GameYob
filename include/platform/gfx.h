#pragma once

#include "types.h"

bool gfxInit();
void gfxCleanup();
bool gfxGetFastForward();
void gfxSetFastForward(bool fastforward);
void gfxLoadBorder(const char* filename);
void gfxLoadBorderBuffer(u8* imgData, u32 imgWidth, u32 imgHeight);
void gfxClearScreen(u8 color);
void gfxClearLine(u32 line, u8 color);
void gfxDrawPixel(int x, int y, u32 pixel);
void gfxDrawScreen();
void gfxFlush();
void gfxWaitForVBlank();
