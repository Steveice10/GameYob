#pragma once

#include <istream>
#include <ostream>

#include "types.h"

class Gameboy;

class PPU {
public:
    PPU(Gameboy* gb);

    void reset();

    void loadState(std::istream& data, u8 version);
    void saveState(std::ostream& data);

    void update();

    void setHalfSpeed(bool halfSpeed);

    void transferTiles(u8* dest);

    inline u8 readOam(u16 addr) {
        return this->oam[addr & 0xFF];
    }

    inline void writeOam(u16 addr, u8 val) {
        this->oam[addr & 0xFF] = val;
    }

    inline u32* getBgPalette() {
        return this->bgPalette;
    }

    inline u32* getSprPalette() {
        return this->sprPalette;
    }
private:
    typedef struct {
        u8 color[8];
        u8 depth[8];
        u8 palette;
    } TileLine;

    typedef struct {
        u8 color[8];
        u8 depth[8];
        u8 x;
        u8 palette;
        u8 obp;
    } SpriteLine;

    void mapBanks();

    void checkLYC();

    void updateLineTile(u8 map, u8 x, u8 y);
    void updateLineSprites();

    void updateScanline();
    void drawPixel(u8 x, u8 y);
    void drawScanline(u8 scanline);

    Gameboy* gameboy;

    u64 lastScanlineCycle;
    u64 lastPhaseCycle;
    bool halfSpeed;

    u8 scanlineX;

    TileLine currTileLines[2];
    SpriteLine currSpriteLines[10];
    u8 currSprites;

    u8 vram[2][0x2000];
    u8 oam[0xA0];
    u8 rawBgPalette[0x40];
    u8 rawSprPalette[0x40];

    u32 bgPalette[0x20];
    u32 sprPalette[0x20];

    u8 expandedBgp[4];
    u8 expandedObp[8];
};
