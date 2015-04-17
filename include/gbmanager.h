#pragma once

#include <time.h>

class Gameboy;

extern Gameboy* gameboy;

extern time_t rawTime;
extern time_t lastRawTime;

void mgrInit();
void mgrRunFrame();

void mgrLoadRom(const char* filename);
void mgrSelectRom();
void mgrSave();

void mgrUpdateVBlank();

void mgrExit();
