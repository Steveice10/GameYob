#pragma once

#include <stdio.h>

#include "types.h"

class MMU {
public:
    MMU(Gameboy* gameboy);

    void reset();

    void loadState(FILE* file, int version);
    void saveState(FILE* file);

    u8 read(u16 addr);
    void write(u16 addr, u8 val);

    inline u8 getWramBank() {
        return this->wramBank;
    }

    inline void setWramBank(u8 bank) {
        this->wramBank = bank;
    }
private:
    Gameboy* gameboy;

    u8 wramBank;

    u8 wram[8][0x1000];
    u8 hram[0x7F];
};