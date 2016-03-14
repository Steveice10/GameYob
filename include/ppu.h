#pragma once

#include "types.h"

class Gameboy;

class PPU {
public:
    PPU(Gameboy* gb);

    void reset();

    void loadState(FILE* file, int version);
    void saveState(FILE* file);

    int update();

    u8 read(u16 addr);
    void write(u16 addr, u8 val);

    void setHalfSpeed(bool halfSpeed);

    void refreshGBPalette();

    void drawScanline(u32 scanline);

    inline u16* getBgPaletteData() {
        return this->bgPaletteData;
    }

    inline u16* getSprPaletteData() {
        return this->sprPaletteData;
    }

    inline u8* getVramBank(u8 bank) {
        return this->vram[bank];
    }
private:
    void checkLYC();
    bool updateHBlankDMA();

    void mapBanks();

    void drawBackground(u16* lineBuffer, u8* depthBuffer, u32 scanline);
    void drawWindow(u16* lineBuffer, u8* depthBuffer, u32 scanline);
    void drawSprites(u16* lineBuffer, u8* depthBuffer, u32 scanline);

    Gameboy* gameboy;

    u64 lastScanlineCycle;
    u64 lastPhaseCycle;
    bool halfSpeed;

    u8 vram[2][0x2000];
    u8 oam[0xA0];

    u16 bgPaletteData[0x20];
    u16 sprPaletteData[0x20];

    u8 lcdc;
    u8 stat;
    u8 scy;
    u8 scx;
    u8 ly;
    u8 lyc;
    u8 sdma;
    u8 bgp;
    u8 obp[2];
    u8 wy;
    u8 wx;
    u8 vramBank;

    u16 dmaSource;
    u16 dmaDest;
    u16 dmaLength;
    int dmaMode;

    u8 bgPaletteSelect;
    u8 sprPaletteSelect;
};
