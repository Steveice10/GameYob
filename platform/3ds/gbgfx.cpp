/* Known graphical issues:
 * DMG sprite order
 * Horizontal window split behavior
 * Needs white screen on LCD disable
 */

#include <3ds.h>
#include <string.h>
#include <math.h>
#include "gbgfx.h"
#include "gameboy.h"
#include "3dsgfx.h"
#include "menu.h"
#include "inputhelper.h"
#include "romfile.h"

#include <ctrcommon/gpu.hpp>
#include <ctrcommon/platform.hpp>

#include "shader_vsh_shbin.h"

// public variables

bool probingForBorder;

int interruptWaitMode;
int scaleMode;
int scaleFilter;
u8 gfxMask;
volatile int loadedBorderType;
bool customBorderExists;
bool sgbBorderLoaded;



// private variables

int lastGameScreen = -1;

u32 gbColors[4];
u32 pixels[32*32*64];

int scale = 3;

u32 bgPalettes[8][4];
u32* bgPalettesRef[8][4];
u32 sprPalettes[8][4];
u32* sprPalettesRef[8][4];

int dmaLine;
bool lineModified;
    

// For drawScanline / drawSprite

u8 spritePixels[256];
u32 spritePixelsTrue[256]; // Holds the palettized colors

u8 spritePixelsLow[256];
u32 spritePixelsTrueLow[256];

u8 bgPixels[256];
u32 bgPixelsTrue[256];
u8 bgPixelsLow[256];
u32 bgPixelsTrueLow[256];


bool bgPalettesModified[8];
bool sprPalettesModified[8];

int prevScaleMode = -1;
int prevGameScreen = -1;

static u8 screenBuffer[256 * 256 * 3];

u32 shader = 0;
u32 texture = 0;
u32 dispVbo = 0;


// Private functions
void drawSprite(int scanline, int spriteNum);

void updateBgPalette(int paletteid);
void updateBgPaletteDMG();
void updateSprPalette(int paletteid);
void updateSprPaletteDMG(int paletteid);


// Function definitions

void doAtVBlank(void (*func)(void)) {
    func();
}

void initGPU() {
    gpuInit();
    gpuCullMode(CULL_BACK_CCW);

    gpuCreateShader(&shader);
    gpuLoadShader(shader, shader_vsh_shbin, shader_vsh_shbin_size);
    gpuUseShader(shader);

    gpuCreateTexture(&texture);
    gpuBindTexture(TEXUNIT0, texture);

    gpuCreateVbo(&dispVbo);
    gpuVboAttributes(dispVbo, ATTRIBUTE(0, 3, ATTR_FLOAT) | ATTRIBUTE(1, 2, ATTR_FLOAT), 2);
}

void initGFX()
{
    bgPalettes[0][0] = RGB24(255, 255, 255);
    bgPalettes[0][1] = RGB24(192, 192, 192);
    bgPalettes[0][2] = RGB24(94, 94, 94);
    bgPalettes[0][3] = RGB24(0, 0, 0);
    sprPalettes[0][0] = RGB24(255, 255, 255);
    sprPalettes[0][1] = RGB24(192, 192, 192);
    sprPalettes[0][2] = RGB24(94, 94, 94);
    sprPalettes[0][3] = RGB24(0, 0, 0);
    sprPalettes[1][0] = RGB24(255, 255, 255);
    sprPalettes[1][1] = RGB24(192, 192, 192);
    sprPalettes[1][2] = RGB24(94, 94, 94);
    sprPalettes[1][3] = RGB24(0, 0, 0);

    for (int i=0; i<8; i++)
    {
        sprPalettesRef[i][0] = &sprPalettes[i][0];
        sprPalettesRef[i][1] = &sprPalettes[i][1];
        sprPalettesRef[i][2] = &sprPalettes[i][2];
        sprPalettesRef[i][3] = &sprPalettes[i][3];

        bgPalettesRef[i][0] = &bgPalettes[i][0];
        bgPalettesRef[i][1] = &bgPalettes[i][1];
        bgPalettesRef[i][2] = &bgPalettes[i][2];
        bgPalettesRef[i][3] = &bgPalettes[i][3];
    }

    memset(bgPalettesModified, 0, sizeof(bgPalettesModified));
    memset(sprPalettesModified, 0, sizeof(sprPalettesModified));
}

void refreshGFX() {
    for (int i=0; i<8; i++) {
        bgPalettesModified[i] = true;
        sprPalettesModified[i] = true;
    }
}

void clearGFX() {

}

void drawScanline(int scanline)
{
}

void drawScanline_P2(int scanline) {
    int tileSigned;
    int BGMapAddr;
    int winMapAddr;

    for (int i=0; i<8; i++) {
        if (bgPalettesModified[i]) {
            if (gameboy->gbMode == GB)
                updateBgPaletteDMG();
            else
                updateBgPalette(i);
            bgPalettesModified[i] = false;
        }
        if (sprPalettesModified[i]) {
            if (gameboy->gbMode == GB)
                updateSprPaletteDMG(i);
            else
                updateSprPalette(i);
            sprPalettesModified[i] = false;
        }
    }

    if (gameboy->ioRam[0x40] & 0x10) {    // Tile Data location
        tileSigned = 0;
    }
    else {
        tileSigned = 1;
    }

    if (gameboy->ioRam[0x40] & 0x8) {        // Tile Map location
        BGMapAddr = 0x1C00;
    }
    else {
        BGMapAddr = 0x1800;
    }
    if (gameboy->ioRam[0x40] & 0x40)
        winMapAddr = 0x1C00;
    else
        winMapAddr = 0x1800;

    /*
    for (int i=0; i<256; i++) {
        //bgPixels[i] = 5;
        //bgPixelsLow[i] = 5;
    }
    */
    memset(bgPixels, 0, sizeof(bgPixels));
    memset(spritePixels, 0, sizeof(spritePixels));
    memset(spritePixelsLow, 0, sizeof(spritePixelsLow));

    if (gameboy->ioRam[0x40] & 0x2) { // Sprites enabled
        for (int i=39; i>=0; i--) {
            drawSprite(scanline, i);
        }
    }

    int winX = gameboy->ioRam[0x4B]-7;
    int winY = gameboy->ioRam[0x4A];
    bool drawingWindow = scanline >= winY && gameboy->ioRam[0x40] & 0x20;

    int BGOn = 1;
    if (!(gameboy->gbMode == CGB) && (gameboy->ioRam[0x40] & 1) == 0)
        BGOn = 0;

    if (BGOn) {
        u8 scrollX = gameboy->ioRam[0x43];
        int scrollY = gameboy->ioRam[0x42];
        // The y position (measured in tiles)
        int tileY = ((scanline+scrollY)&0xFF)/8;

        int numTilesX = 20;
        if (drawingWindow)
            numTilesX = (winX+7)/8;
        int startTile = scrollX/8;
        int endTile = (startTile+numTilesX+1)&31;

        for (int i=startTile; i!=endTile; i=(i+1)&31)
        {
            int mapAddr = BGMapAddr+i+(tileY*32);        // The address (from beginning of gameboy->vram) of the tile's mapping
            // This is the tile id.
            int tileNum = gameboy->vram[0][mapAddr];
            if (tileSigned)
                tileNum = ((s8)tileNum)+256;

            int flipX = 0, flipY = 0;
            int bank = 0;
            int paletteid = 0;
            int priority = 0;

            if (gameboy->gbMode == CGB)
            {
                flipX = gameboy->vram[1][mapAddr] & 0x20;
                flipY = gameboy->vram[1][mapAddr] & 0x40;
                bank = !!(gameboy->vram[1][mapAddr] & 0x8);
                paletteid = gameboy->vram[1][mapAddr] & 0x7;
                priority = gameboy->vram[1][mapAddr] & 0x80;
            }

            // This is the tile's Y position to be read (0-7)
            int pixelY = (scanline+scrollY)%8;
            if (flipY)
                pixelY = 7-pixelY;

            for (int x=0; x<8; x++)
            {
                int colorid;
                u32 color;

                if (flipX)
                {
                    colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>(7-x)));
                    colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>(7-x)))<<1;
                }
                else
                {
                    colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>x));
                    colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>x))<<1;
                }
                // The x position to write to pixels[].
                u32 writeX = ((i*8)+x-scrollX)&0xFF;

                color = *bgPalettesRef[paletteid][colorid];
                if (priority) {
                    bgPixels[writeX] = colorid;
                    bgPixelsTrue[writeX] = color;
                    bgPixelsLow[writeX] = colorid;
                    bgPixelsTrueLow[writeX] = color;
                }
                else {
                    bgPixelsLow[writeX] = colorid;
                    bgPixelsTrueLow[writeX] = color;
                }
                //spritePixels[writeX] = 0;
            }
        }
    }

    // Draw window

    if (drawingWindow)
    {
        int tileY = (scanline-winY)/8;

        int endTile = 21-winX/8;
        for (int i=0; i<endTile; i++)
        {
            int mapAddr = winMapAddr+i+(tileY*32);
            // This is the tile id.
            int tileNum = gameboy->vram[0][mapAddr];
            if (tileSigned)
                tileNum = ((s8)tileNum)+128+0x80;

            int flipX = 0, flipY = 0;
            int bank = 0;
            int paletteid = 0;
            int priority = 0;

            if (gameboy->gbMode == CGB)
            {
                flipX = gameboy->vram[1][mapAddr] & 0x20;
                flipY = gameboy->vram[1][mapAddr] & 0x40;
                bank = !!(gameboy->vram[1][mapAddr]&0x8);
                paletteid = gameboy->vram[1][mapAddr]&0x7;
                priority = gameboy->vram[1][mapAddr] & 0x80;
            }

            int pixelY = (scanline-winY)%8;
            if (flipY)
                pixelY = 7-pixelY;
            for (int x=0; x<8; x++)
            {
                int colorid;
                u32 color;

                if (flipX)
                {
                    colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>(7-x)));
                    colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>(7-x)))<<1;
                }
                else
                {
                    colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>x));
                    colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>x))<<1;
                }

                int writeX = (i*8)+x+winX;

                color = *bgPalettesRef[paletteid][colorid];
                if (priority)
                {
                    bgPixels[writeX] = colorid;
                    bgPixelsTrue[writeX] = color;
                    bgPixelsLow[writeX] = colorid;
                    bgPixelsTrueLow[writeX] = color;
                }
                else
                {
                    bgPixelsLow[writeX] = colorid;
                    bgPixelsTrueLow[writeX] = color;
                    bgPixels[writeX] = 0;
                }
            }
        }
    }

    // TODO: bg priority cancellation

    int y = scanline;
    for (int i=0; i<160; i++)
    {
        u32 pixel;
        if (bgPixels[i] != 0)
            pixel = bgPixelsTrue[i];
        else if (spritePixels[i] != 0)
            pixel = spritePixelsTrue[i];
        else if (spritePixelsLow[i] == 0 || bgPixelsLow[i] != 0)
            pixel = bgPixelsTrueLow[i];
        else
            pixel = spritePixelsTrueLow[i];

        *(u16*) &screenBuffer[(y * 256 + i) * 3] = (u16) pixel;
        screenBuffer[(y * 256 + i) * 3 + 2] = (u8) (pixel >> 16);
    }
}

void drawSprite(int scanline, int spriteNum)
{
    // The sprite's number, times 4 (each uses 4 bytes)
    spriteNum *= 4;

    int y = (gameboy->hram[spriteNum]-16);
    int height;
    if (gameboy->ioRam[0x40] & 0x4)
        height = 16;
    else
        height = 8;

    if (scanline < y || scanline >= y+height)
        return;

    int x = (gameboy->hram[spriteNum+1]-8);
    int tileNum = gameboy->hram[spriteNum+2];
    int bank = 0;
    int flipX = (gameboy->hram[spriteNum+3] & 0x20);
    int flipY = (gameboy->hram[spriteNum+3] & 0x40);
    int priority = !(gameboy->hram[spriteNum+3] & 0x80);
    int paletteid;

    if (gameboy->gbMode == CGB)
    {
        bank = (gameboy->hram[spriteNum+3]&0x8)>>3;
        paletteid = gameboy->hram[spriteNum+3] & 0x7;
    }
    else
    {
        //paletteid = gameboy->hram[spriteNum+3] & 0x7;
        paletteid = (gameboy->hram[spriteNum+3] & 0x10)>>4;
    }

    if (height == 16) {
        tileNum &= ~1;

        if (scanline-y >= 8)
            tileNum++;
    }

    //u8* tile = &memory[0]+((tileNum)*16)+0x8000;//tileAddr;
    int pixelY = (scanline-y)%8;

    if (flipY)
    {
        pixelY = 7-pixelY;
        if (height == 16)
            tileNum = tileNum^1;
    }
    u32* trueDest = (priority ? spritePixelsTrue : spritePixelsTrueLow);
    u8* idDest = (priority ? spritePixels : spritePixelsLow);
    for (int j=0; j<8; j++)
    {
        int color;
        int trueColor;

        color = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>j));
        color |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>j))<<1;
        if (color != 0)
        {
            trueColor = *sprPalettesRef[paletteid][color];

            if (flipX)
            {
                idDest[(x+(7-j))&0xFF] = color;
                trueDest[(x+(7-j))&0xFF] = trueColor;
            }
            else
            {
                idDest[(x+j)&0xFF] = color;
                trueDest[(x+j)&0xFF] = trueColor;
            }
        }
    }
}

extern u8 gfxBuffer;

void drawScreen()
{
    int fbWidth = gameScreen == 0 ? TOP_SCREEN_WIDTH : BOTTOM_SCREEN_WIDTH;
    int fbHeight = gameScreen == 0 ? TOP_SCREEN_HEIGHT : BOTTOM_SCREEN_HEIGHT;

    float wScale = 1;
    float hScale = 1;
    if(scaleMode == 1) {
        float scale = fbHeight / (float) 144;
        wScale = scale;
        hScale = scale;
    } else if(scaleMode == 2) {
        wScale = fbWidth / (float) 160;
        hScale = fbHeight / (float) 144;
    }

    // Update VBO data if the size has changed.
    if(prevScaleMode != scaleMode || prevGameScreen != gameScreen) {
        prevScaleMode = scaleMode;
        prevGameScreen = gameScreen;

        u32 vboWidth = (u32) (160 * wScale);
        u32 vboHeight = (u32) (144 * hScale);

        // Calculate VBO extents.
        float horizExtent = vboWidth < 400 ? (float) vboWidth / 400.0f : 1;
        float vertExtent = vboHeight < 240 ? (float) vboHeight / 240.0f : 1;

        // Adjust for power-of-two textures.
        static float horizMod = (float) (256 - 160) / (float) 256;
        static float vertMod = (float) (256 - 144) / (float) 256;

        // Prepare new VBO data.
        const float vbo[] = {
                -vertExtent, -horizExtent, -0.1f, 1.0f - horizMod, 0.0f + vertMod,
                vertExtent, -horizExtent, -0.1f, 1.0f - horizMod, 1.0f,
                vertExtent, horizExtent, -0.1f, 0.0f, 1.0f,
                vertExtent, horizExtent, -0.1f, 0.0f, 1.0f,
                -vertExtent, horizExtent, -0.1f, 0.0f, 0.0f + vertMod,
                -vertExtent, -horizExtent, -0.1f, 1.0f - horizMod, 0.0f + vertMod,
        };

        // Update the VBO with the new data.
        gpuVboData(dispVbo, vbo, sizeof(vbo), sizeof(vbo) / (5 * 4), PRIM_TRIANGLES);
    }

    gpuViewport(gameScreen == 0 ? TOP_SCREEN : BOTTOM_SCREEN, 0, 0, (u32) fbHeight, (u32) fbWidth);

    // Update the texture with the new frame.
    gpuTextureData(texture, screenBuffer, 256, 256, PIXEL_RGB8, 256, 256, PIXEL_RGB8, TEXTURE_MIN_FILTER(FILTER_LINEAR) | TEXTURE_MAG_FILTER(FILTER_LINEAR));

    gpuClear();
    gpuDrawVbo(dispVbo);
    gpuFlush();
    gpuSwapBuffers(true);

    gfxBuffer ^= 1;
    consoleCheckFramebuffers();
}


void displayIcon(int iconid) {

}


void selectBorder() {

}

int loadBorder(const char* filename) {
    return 0;
}

void checkBorder() {
    if (lastGameScreen != gameScreen) {
        lastGameScreen = gameScreen;
        u8** buffers;
        if (gameScreen == 0)
            buffers = gfxTopLeftFramebuffers;
        else
            buffers = gfxBottomFramebuffers;
        for (int fb=0; fb<2; fb++) {
            memset(buffers[fb], 0, framebufferSizes[gameScreen]);
        }
    }
}

void refreshScaleMode() {

}


// SGB stub functions
void refreshSgbPalette() {

}
void setSgbMask(int mask) {

}
void setSgbTiles(u8* src, u8 flags) {

}
void setSgbMap(u8* src) {

}


void writeVram(u16 addr, u8 val) {
}

void writeVram16(u16 addr, u16 src) {
}

void writeHram(u16 addr, u8 val) {
}

void handleVideoRegister(u8 ioReg, u8 val) {
    switch(ioReg) {
        case 0x47:
            if (gameboy->gbMode == GB)
                bgPalettesModified[0] = true;
            break;
        case 0x48:
            if (gameboy->gbMode == GB)
                sprPalettesModified[0] = true;
            break;
        case 0x49:
            if (gameboy->gbMode == GB)
                sprPalettesModified[1] = true;
            break;
        case 0x69:
            if (gameboy->gbMode == CGB)
                bgPalettesModified[(gameboy->ioRam[0x68]/8)&7] = true;
            break;
        case 0x6B:
            if (gameboy->gbMode == CGB)
                sprPalettesModified[(gameboy->ioRam[0x6A]/8)&7] = true;
            break;
        default:
            break;
    }
}

void updateBgPalette(int paletteid)
{
    int multiplier = 8;
    int i;
    for (i=0; i<4; i++)
    {
        int red = (gameboy->bgPaletteData[(paletteid*8)+(i*2)]&0x1F)*multiplier;
        int green = (((gameboy->bgPaletteData[(paletteid*8)+(i*2)]&0xE0) >> 5) |
                ((gameboy->bgPaletteData[(paletteid*8)+(i*2)+1]) & 0x3) << 3)*multiplier;
        int blue = ((gameboy->bgPaletteData[(paletteid*8)+(i*2)+1] >> 2) & 0x1F)*multiplier;
        bgPalettes[paletteid][i] = RGB24(red, green, blue);
    }
}

void updateBgPaletteDMG()
{
    u8 val = gameboy->ioRam[0x47];
    u8 palette[] = {val&3, (val>>2)&3, (val>>4)&3, (val>>6)};

    int paletteid = 0;
    int multiplier = 8;
    for (int i=0; i<4; i++) {
        u8 col = palette[i];
        int red = (gameboy->bgPaletteData[(paletteid*8)+(col*2)]&0x1F)*multiplier;
        int green = (((gameboy->bgPaletteData[(paletteid*8)+(col*2)]&0xE0) >> 5) |
                ((gameboy->bgPaletteData[(paletteid*8)+(col*2)+1]) & 0x3) << 3)*multiplier;
        int blue = ((gameboy->bgPaletteData[(paletteid*8)+(col*2)+1] >> 2) & 0x1F)*multiplier;

        bgPalettes[0][i] = RGB24(red, green, blue);
    }
}

void updateSprPalette(int paletteid)
{
    int multiplier = 8;
    int i;
    for (i=0; i<4; i++)
    {
        int red = (gameboy->sprPaletteData[(paletteid*8)+(i*2)]&0x1F)*multiplier;
        int green = (((gameboy->sprPaletteData[(paletteid*8)+(i*2)]&0xE0) >> 5) |
                ((gameboy->sprPaletteData[(paletteid*8)+(i*2)+1]) & 0x3) << 3)*multiplier;
        int blue = ((gameboy->sprPaletteData[(paletteid*8)+(i*2)+1] >> 2) & 0x1F)*multiplier;
        sprPalettes[paletteid][i] = RGB24(red, green, blue);
    }
}

void updateSprPaletteDMG(int paletteid)
{
    u8 val = gameboy->ioRam[0x48+paletteid];
    u8 palette[] = {val&3, (val>>2)&3, (val>>4)&3, (val>>6)};

    int multiplier = 8;
    for (int i=0; i<4; i++) {
        u8 col = palette[i];
        int red = (gameboy->sprPaletteData[(paletteid*8)+(col*2)]&0x1F)*multiplier;
        int green = (((gameboy->sprPaletteData[(paletteid*8)+(col*2)]&0xE0) >> 5) |
                ((gameboy->sprPaletteData[(paletteid*8)+(col*2)+1]) & 0x3) << 3)*multiplier;
        int blue = ((gameboy->sprPaletteData[(paletteid*8)+(col*2)+1] >> 2) & 0x1F)*multiplier;
        sprPalettes[paletteid][i] = RGB24(red, green, blue);
    }
}

