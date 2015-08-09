#pragma once

#include "types.h"

class Gameboy;

enum {
    BORDER_NONE = 0,
    BORDER_SGB,
    BORDER_CUSTOM
};  // Use with loadedBorderType

class GameboyPPU {
public:
    GameboyPPU(Gameboy* gb);

    void initPPU();
    void refreshPPU();
    void clearPPU();

    void drawScanline(int scanline);
    void drawScreen();

    void setSgbMask(int mask);
    void setSgbTiles(u8* src, u8 flags);
    void setSgbMap(u8* src);

    void writeVram(u16 addr, u8 val);
    void writeVram16(u16 addr, u16 src);
    void writeHram(u16 addr, u8 val);
    void handleVideoRegister(u8 ioReg, u8 val);

    void updateBgPalette(int paletteid);
    void updateBgPaletteDMG();
    void updateSprPalette(int paletteid);
    void updateSprPaletteDMG(int paletteid);

    bool probingForBorder;
    
    u8 gfxMask;
    volatile int loadedBorderType;
    bool customBorderExists;
    bool sgbBorderLoaded;
    int fastForwardFrameSkip = 0;

private:
    void drawBackground(int scanline, int winX, int winY, bool drawingWindow, bool tileSigned);
    void drawWindow(int scanline, int winX, int winY, bool drawingWindow, bool tileSigned);
    void drawSprites(int scanline);

    Gameboy* gameboy;

    int fastForwardCounter = 0;

    u32 bgPalettes[8][4];
    u32 sprPalettes[8][4];

// For drawScanline / drawSprite

    u32 depthBuffer[256];
    u32 *lineBuffer;
    u8 *subSgbMap;

    bool bgPalettesModified[8];
    bool sprPalettesModified[8];
};
