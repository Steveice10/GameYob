#pragma once

#include <ctrcommon/types.hpp>

extern bool gbsMode;
extern u8 gbsHeader[0x70];

extern u8 gbsNumSongs;
extern u16 gbsLoadAddress;
extern u16 gbsInitAddress;
extern u16 gbsPlayAddress;

void gbsReadHeader();
void gbsInit();
void gbsCheckInput();
