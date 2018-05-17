#pragma once

#include "types.h"

struct KeyConfig {
    std::string name;
    u8 funcKeys[512];
};

bool configLoad();
void configSave();

std::string& configGetRomPath();

std::string& configGetBorderPath();
void configSetBorderPath(std::string& path);

u32 configGetKeyConfigCount();
KeyConfig* configGetKeyConfig(u32 num);
void configAddKeyConfig(const std::string& name);
void configRemoveKeyConfig(u32 num);

u32 configGetSelectedKeyConfig();
void configSetSelectedKeyConfig(u32 config);

const char* configGetFuncKeyName(u8 funcKey);