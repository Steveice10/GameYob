#pragma once

#include "types.h"

class CheatEngine;
class Gameboy;

extern Gameboy* gameboy;
extern CheatEngine* cheatEngine;

extern u8 gbBios[0x200];
extern bool gbBiosLoaded;
extern u8 gbcBios[0x900];
extern bool gbcBiosLoaded;

extern int fastForwardCounter;

void mgrInit();
void mgrExit();

void mgrReset(bool initial = false);

std::string mgrGetRomName();
void mgrLoadRom(const char* filename);
void mgrUnloadRom(bool save = true);
void mgrSelectRom();

void mgrLoadSave();
void mgrWriteSave();

bool mgrStateExists(int stateNum);
bool mgrLoadState(int stateNum);
bool mgrSaveState(int stateNum);
void mgrDeleteState(int stateNum);

void mgrRefreshBorder();

void mgrRefreshBios();

void mgrPause();
void mgrUnpause();
bool mgrIsPaused();

void mgrRun();
