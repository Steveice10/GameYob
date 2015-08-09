#pragma once

#include "types.h"

#ifdef BACKEND_3DS
    #define NUM_BUTTONS 32
#elif defined(BACKEND_SDL)
    #define NUM_BUTTONS 512
#else
    #define NUM_BUTTONS 0
#endif

struct KeyConfig {
    char name[32];
    u8 funcKeys[NUM_BUTTONS];
};

extern std::string gbBiosPath;
extern std::string gbcBiosPath;
extern std::string romPath;
extern std::string borderPath;

bool readConfigFile();
void writeConfigFile();

void startKeyConfigChooser();

