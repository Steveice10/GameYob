#pragma once

#include <time.h>

#include "gameboy.h"

extern Gameboy* gameboy;

void mgrInit();
void mgrExit();

void mgrLoadRom(const char* filename);
void mgrSelectRom();
void mgrSave();

void mgrRun();
