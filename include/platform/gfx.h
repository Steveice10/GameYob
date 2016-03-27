#pragma once

#include "types.h"

bool gfxInit();
void gfxCleanup();
void gfxLoadBorder(u8* imgData, int imgWidth, int imgHeight);
u32* gfxGetLineBuffer(u32 line);
void gfxClearScreenBuffer(u32 rgba);
void gfxClearLineBuffer(u32 line, u32 rgba);
void gfxTakeScreenshot();
void gfxDrawScreen();
