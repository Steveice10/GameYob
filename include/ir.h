#pragma once

#include <stdio.h>

#include "types.h"

class IR {
public:
    IR(Gameboy* gameboy);

    void reset();

    void loadState(FILE* file, int version);
    void saveState(FILE* file);

    u8 read(u16 addr);
    void write(u16 addr, u8 val);
private:
    Gameboy* gameboy;

    u8 rp;
};