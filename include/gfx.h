#pragma once

#include <ctrcommon/types.hpp>

void gfxInit();
void gfxCleanup();
void gfxDrawScreen(u8* screenBuffer, int scaleMode, int gameScreen);
void gfxFlush();
void gfxWaitForVBlank();
