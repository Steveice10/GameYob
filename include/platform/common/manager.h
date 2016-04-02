#pragma once

#include "types.h"

class CheatEngine;
class Gameboy;

extern Gameboy* gameboy;
extern CheatEngine* cheatEngine;

extern int fastForwardCounter;

void mgrInit();
void mgrExit();

void mgrReset();

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

bool mgrGetFastForward();
void mgrSetFastForward(bool ff);
void mgrToggleFastForward();

void mgrPause();
void mgrUnpause();
bool mgrIsPaused();

void mgrRun();
