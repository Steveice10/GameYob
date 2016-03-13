#pragma once

#include <stdio.h>

#include "types.h"

class Serial {
public:
    Serial(Gameboy* gameboy);
    ~Serial();

    void reset();

    void loadState(FILE* file, int version);
    void saveState(FILE* file);

    int update();

    u8 read(u16 addr);
    void write(u16 addr, u8 val);
private:
    Gameboy* gameboy;

    Printer* printer;

    u64 nextSerialInternalCycle;
    u64 nextSerialExternalCycle;

    u8 sb;
    u8 sc;
};