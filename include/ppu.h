#pragma once

#include <ctrcommon/types.hpp>

class Gameboy;

enum Icons {
    ICON_NULL = 0,
    ICON_PRINTER
};

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
    void drawScanline_P2(int scanline);
    void drawScreen();

    void displayIcon(int iconid);

    void selectBorder(); // Starts the file chooser for a border
    int loadBorder(const char* filename); // Loads the border to vram
    void checkBorder(); // Decides what kind of border to use, invokes loadBorder if necessary

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

private:
    void drawSprite(int scanline, int spriteNum);

    void updateBgPalette(int paletteid);
    void updateBgPaletteDMG();
    void updateSprPalette(int paletteid);
    void updateSprPaletteDMG(int paletteid);

    Gameboy* gameboy;

    int lastGameScreen = -1;

    u32 gbColors[4];
    u32 pixels[32 * 32 * 64];

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
};
