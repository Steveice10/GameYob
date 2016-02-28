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

    bool probingForBorder;
    
    u8 gfxMask;
    volatile int loadedBorderType;
    bool customBorderExists;
    bool sgbBorderLoaded;
    int fastForwardFrameSkip = 0;

private:
    inline u16 getBgColor(u32 paletteId, u32 colorId);
    inline u16 getSprColor(u32 paletteId, u32 colorId);

    void drawBackground(u16* lineBuffer, u8* depthBuffer, int scanline, int winX, int winY, bool drawingWindow, bool tileSigned);
    void drawWindow(u16* lineBuffer, u8* depthBuffer, int scanline, int winX, int winY, bool drawingWindow, bool tileSigned);
    void drawSprites(u16* lineBuffer, u8* depthBuffer, int scanline);

    Gameboy* gameboy;

    int fastForwardCounter = 0;
};
