#pragma once
#include <3ds/gfx.h>

void initGPU();
void deinitGPU();
void drawGPU(u8* screenBuffer, int scaleMode, int gameScreen);

u32 getPixel(u8* framebuffer, int x, int y);
void drawPixel(u8* framebuffer, int x, int y, u32 color);

u8* gfxGetActiveFramebuffer(gfxScreen_t screen, gfx3dSide_t side);
u8* gfxGetInactiveFramebuffer(gfxScreen_t screen, gfx3dSide_t side);
void gfxMySwapBuffers();

inline u32 RGB24(int red, int green, int blue) {
    return red<<16 | green<<8 | blue;
}

const int TOP_SCREEN_WIDTH = 400;
const int TOP_SCREEN_HEIGHT = 240;

const int BOTTOM_SCREEN_WIDTH = 320;
const int BOTTOM_SCREEN_HEIGHT = 240;

const int framebufferSizes[] = {288000, 230400};
