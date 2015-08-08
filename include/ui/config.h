#pragma once

#include "platform/input.h"

#define NUM_FUNC_KEYS 19

struct KeyConfig {
    char name[32];
    FuncKey funcKeys[NUM_BUTTONS];
};

extern char gbBiosPath[512];
extern char gbcBiosPath[512];
extern char romPath[512];
extern char borderPath[512];

bool readConfigFile();
void writeConfigFile();

void startKeyConfigChooser();

