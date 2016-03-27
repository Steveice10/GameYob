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

extern u16 gbBgPalette[0x20];
extern u16 gbSprPalette[0x20];

extern int fastForwardCounter;

void mgrInit();
void mgrExit();

void mgrReset(bool bios = true);

std::string mgrGetRomName();
void mgrPowerOn(const char* romFile);
void mgrPowerOff(bool save = true);
void mgrSelectRom();

void mgrWriteSave();

bool mgrStateExists(int stateNum);
bool mgrLoadState(int stateNum);
bool mgrSaveState(int stateNum);
void mgrDeleteState(int stateNum);

void mgrRefreshPalette();
void mgrRefreshBorder();
void mgrRefreshBios();

bool mgrGetFastForward();
void mgrSetFastForward(bool ff);
void mgrToggleFastForward();

void mgrPause();
void mgrUnpause();
bool mgrIsPaused();

void mgrRun();
