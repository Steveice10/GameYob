#pragma once

#include "types.h"

bool gfxInit();
void gfxCleanup();
bool gfxGetFastForward();
void gfxSetFastForward(bool fastforward);
void gfxLoadBorder(const char* filename);
void gfxLoadBorderBuffer(u8* imgData, u32 imgWidth, u32 imgHeight);
u32* gfxGetScreenBuffer();
void gfxDrawScreen();
void gfxFlush();
void gfxWaitForVBlank();
