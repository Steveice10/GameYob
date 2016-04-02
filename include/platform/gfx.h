#pragma once

#include "types.h"

bool gfxInit();
void gfxCleanup();
void gfxLoadBorder(u8* imgData, int imgWidth, int imgHeight);
u32* gfxGetScreenBuffer();
u32 gfxGetScreenPitch();
void gfxTakeScreenshot();
void gfxDrawScreen();
