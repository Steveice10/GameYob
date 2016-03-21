#pragma once

#include <istream>
#include <ostream>

#include "types.h"

class Serial {
public:
    Serial(Gameboy* gameboy);
    ~Serial();

    void reset();

    void loadState(std::istream& data, u8 version);
    void saveState(std::ostream& data);

    void update();
private:
    Gameboy* gameboy;

    Printer* printer;

    u64 nextSerialInternalCycle;
    u64 nextSerialExternalCycle;
};