#pragma once

#include <stdio.h>

#include "types.h"

class Timer {
public:
    Timer(Gameboy* gameboy);

    void reset();

    void loadState(FILE* file, int version);
    void saveState(FILE* file);

    void update();
private:
    Gameboy* gameboy;

    u64 lastDividerCycle;
    u64 lastTimerCycle;
};