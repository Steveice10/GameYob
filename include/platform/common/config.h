#pragma once

#include "types.h"

struct KeyConfig {
    char name[32];
    u8 funcKeys[512];
};

extern std::string romPath;
extern std::string borderPath;

bool readConfigFile();
void writeConfigFile();

void startKeyConfigChooser();

