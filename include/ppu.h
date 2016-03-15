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

    inline u8* getOam() {
        return this->oam;
    }
private:
    void checkLYC();

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
};
