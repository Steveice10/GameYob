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

    u8 read(u16 addr);
    void write(u16 addr, u8 val);
private:
    void updateDivider();
    void updateTimer();

    Gameboy* gameboy;

    u64 lastDividerCycle;
    u64 lastTimerCycle;
    u32 timerCatchUpCycles;

    u8 dividerCounter;
    bool timerEnabled;
    u8 timerPeriod;
    u8 timerCounter;
    u8 timerModulo;
};