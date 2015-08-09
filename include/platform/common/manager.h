#pragma once

class Gameboy;
extern Gameboy* gameboy;

void mgrInit();
void mgrExit();

void mgrLoadRom(const char* filename);
void mgrSelectRom();
void mgrSave();

void mgrRefreshBorder();

void mgrRefreshBios();

bool mgrStateExists(int stateNum);
bool mgrLoadState(int stateNum);
bool mgrSaveState(int stateNum);
void mgrDeleteState(int stateNum);

void mgrRun();
