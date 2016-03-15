#pragma once

#include <stdio.h>

#include "types.h"

class Gameboy;

class Gb_Apu;
class Stereo_Buffer;

class APU {
public:
    APU(Gameboy* gameboy);
    ~APU();

    void reset();

    void loadState(FILE* file, int version);
    void saveState(FILE* file);

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