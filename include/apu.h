#pragma once

#include <istream>
#include <ostream>

#include "types.h"

class Gameboy;

class Gb_Apu;
class Stereo_Buffer;

class APU {
public:
    APU(Gameboy* gameboy);
    ~APU();

    void reset();

    void loadState(std::istream& data, u8 version);
    void saveState(std::ostream& data);

    void update();

    void setHalfSpeed(bool halfSpeed);

    void setChannelEnabled(int channel, bool enabled);
private:
    Gameboy* gameboy;

    Stereo_Buffer* buffer;

    Gb_Apu* apu;

    u64 lastSoundCycle;
    bool halfSpeed;
};