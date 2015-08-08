#pragma once

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

extern char gbBiosPath[512];
extern char gbcBiosPath[512];
extern char romPath[512];
extern char borderPath[512];

bool readConfigFile();
void writeConfigFile();

void startKeyConfigChooser();

