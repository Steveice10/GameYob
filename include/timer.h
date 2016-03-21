#pragma once

#include <istream>
#include <ostream>

#include "types.h"

class Timer {
public:
    Timer(Gameboy* gameboy);

    void reset();

    void loadState(std::istream& data, u8 version);
    void saveState(std::ostream& data);

    void update();
private:
    Gameboy* gameboy;

    u64 lastDividerCycle;
    u64 lastTimerCycle;
};