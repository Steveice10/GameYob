#pragma once

#include <ctrcommon/types.hpp>

class Gameboy;

#define CHAN_1 1
#define CHAN_2 2
#define CHAN_3 4
#define CHAN_4 8

#define CYCLES_UNTIL_SAMPLE (0x54)

class GameboyAPU {
public:
    GameboyAPU(Gameboy* g);
    ~GameboyAPU();
    void setGameboy(Gameboy* g);

    void init();
    void refresh();
    void mute();
    void unmute();

    void enableChannel(int i);
    void disableChannel(int i);

    void updateSound(int cycles);

    void setSoundEventCycles(int cycles); // Should be moved out of here
    void soundUpdate();
    void handleSoundRegister(u8 ioReg, u8 val);

    int cyclesToSoundEvent;

private:

    double chan4FreqRatio;
    int chan1SweepTime;
    int chan1SweepCounter;
    int chan1SweepDir;
    int chan1SweepAmount;
    int chanLen[4];
    int chanLenCounter[4];
    int chanUseLen[4];
    u32 chanFreq[4];
    int chanVol[4];
    int chanEnvDir[4];
    int chanEnvCounter[4];
    int chanEnvSweep[4];
    u16 lfsr;
    int noiseVal;
    int chan3WavPos;
    int chanPolarity[4];
    int chanPolarityCounter[4];

    int chan4Width;
    int chanDuty[2];
    int chanFreqClocks[4];
    int chanOn[4];
    int chanToOut1[4];
    int chanToOut2[4];

    int SO1Vol;
    int SO2Vol;

    int cyclesUntilSample;

    bool chanEnabled[4] = {true, true, true, true};

    bool muted;

    Gameboy* gameboy;
};
