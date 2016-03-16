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

    void transferTiles(u8* dest);
    void drawScanline(u32 scanline);

    inline u8 readOAM(u16 addr) {
        return this->oam[addr & 0xFF];
    }

    inline void writeOAM(u16 addr, u8 val) {
        this->oam[addr & 0xFF] = val;
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

    u8 tiles[2][384][8 * 8];
};
