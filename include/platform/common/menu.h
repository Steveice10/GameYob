#pragma once

#include <stdio.h>

#include <vector>

#include "types.h"

extern std::vector<std::string> supportedImages;

extern int borderSetting;
extern int pauseOnMenu;
extern int stateNum;
extern int gameScreen;
extern int scaleMode;
extern int scaleFilter;
extern int borderScaleMode;
extern bool fpsOutput;
extern bool timeOutput;
extern int gbColorizeMode;
extern u16 gbBgPalette[0x20];
extern u16 gbSprPalette[0x20];
extern bool printerEnabled;
extern bool soundEnabled;
extern int sgbModeOption;
extern int gbcModeOption;
extern bool gbaModeOption;
extern int biosMode;
extern int fastForwardFrameSkip;
extern FILE* linkSocket;

void setMenuDefaults();

void displayMenu();
void closeMenu(); // updateScreens may need to be called after this
bool isMenuOn();

void redrawMenu();
void updateMenu();
void printMenuMessage(const char* s);

void displaySubMenu(void (* updateFunc)());
void closeSubMenu();

int getMenuOption(const char* name);
void setMenuOption(const char* name, int value);
void enableMenuOption(const char* name);
void disableMenuOption(const char* name);

void menuParseConfig(char* line);
const std::string menuPrintConfig();

bool showConsoleDebug();

