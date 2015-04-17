#pragma once

#include <ctrcommon/types.hpp>

bool gfxInit();
void gfxCleanup();
void gfxToggleFastForward();
void gfxSetFastForward(bool fastforward);
void gfxDrawPixel(int x, int y, u32 pixel);
void gfxDrawScreen();
void gfxFlush();
void gfxWaitForVBlank();
void gfxClearScreens();
