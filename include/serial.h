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

    void update();
private:
    Gameboy* gameboy;

    Printer* printer;

    u64 nextSerialInternalCycle;
    u64 nextSerialExternalCycle;
};