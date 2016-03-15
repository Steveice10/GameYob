#pragma once

#include <stdio.h>

#include "types.h"

class APU;
class CPU;
class MBC;
class MMU;
class PPU;
class Printer;
class RomFile;
class Serial;
class SGB;
class Timer;

// Cycle Constants
#define CYCLES_PER_SECOND 4194304
#define CYCLES_PER_FRAME 70224

// Return Codes
#define RET_VBLANK 1
#define RET_LINK 2

// Buttons
#define GB_A 0x01
#define GB_B 0x02
#define GB_SELECT 0x04
#define GB_START 0x08
#define GB_RIGHT 0x10
#define GB_LEFT 0x20
#define GB_UP 0x40
#define GB_DOWN 0x80

typedef enum {
    MODE_GB,
    MODE_SGB,
    MODE_CGB
} GBMode;

class Gameboy {
public:
    Gameboy();
    ~Gameboy();

    void reset(bool allowBios = true);

    bool loadState(FILE* file);
    bool saveState(FILE* file);

    int run();

    bool isRomLoaded();
    bool loadRomFile(const char* filename);
    void unloadRom();

    RomFile* romFile;

    CPU* cpu;
    Timer* timer;
    MMU* mmu;
    PPU* ppu;
    APU* apu;
    SGB* sgb;
    MBC* mbc;
    Serial* serial;

    GBMode gbMode;
    bool biosOn;
private:
    static int pollEvents(Gameboy* gameboy);
};