#pragma once
#include <3ds/gfx.h>

const int TOP_SCREEN_WIDTH = 400;
const int TOP_SCREEN_HEIGHT = 240;

const int BOTTOM_SCREEN_WIDTH = 320;
const int BOTTOM_SCREEN_HEIGHT = 240;

const int framebufferSizes[] = {288000, 230400};

void gfxInit();
void gfxCleanup();
void gfxDrawScreen(u8 *screenBuffer, int scaleMode, int gameScreen);
void gfxWaitForVBlank();
