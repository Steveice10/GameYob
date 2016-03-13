#pragma once

#include "types.h"

bool gfxInit();
void gfxCleanup();
bool gfxGetFastForward();
void gfxSetFastForward(bool fastforward);
void gfxToggleFastForward();
void gfxLoadBorder(u8* imgData, u32 imgWidth, u32 imgHeight);
u16* gfxGetLineBuffer(int line);
void gfxClearScreenBuffer(u16 rgba5551);
void gfxClearLineBuffer(int line, u16 rgba5551);
void gfxDrawScreen();
void gfxSync();
