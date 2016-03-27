#pragma once

#include <istream>
#include <ostream>

#include "types.h"

class APU;
class CPU;
class Cartridge;
class MMU;
class PPU;
class Printer;
class Serial;
class SGB;
class Timer;

// Cycle Constants
#define CYCLES_PER_SECOND 4194304
#define CYCLES_PER_FRAME 70224

// Return Codes
#define RET_VBLANK 1

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

inline std::istream& operator>>(std::istream& str, GBMode& v) {
    u8 mode = 0;
    if(str >> mode) {
        v = static_cast<GBMode>(mode);
    }

    return str;
}

inline std::ostream& operator<<(std::ostream& str, GBMode v) {
    str << (u8) v;
    return str;
}

class Gameboy {
public:
    Gameboy();
    ~Gameboy();

    void powerOn(bool bios = true);
    void powerOff();

    bool loadState(std::istream& data);
    bool saveState(std::ostream& data);

    int run();

    inline bool isPoweredOn() {
        return this->poweredOn;
    }

    Cartridge* cartridge;

    MMU* mmu;
    CPU* cpu;
    PPU* ppu;
    APU* apu;
    SGB* sgb;
    Timer* timer;
    Serial* serial;

    int frontendEvents;

    GBMode gbMode;

    bool biosOn;
    GBMode biosType;
private:
    bool poweredOn = false;
};