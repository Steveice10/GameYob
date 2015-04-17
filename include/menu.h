#pragma once

#include <stdio.h>

extern bool consoleDebugOutput;
extern int gbcModeOption;
extern bool gbaModeOption;
extern int sgbModeOption;
extern bool soundDisabled;
extern int gbColorize;
extern bool customBordersEnabled;
extern bool sgbBordersEnabled;
extern bool autoSavingEnabled;
extern bool printerEnabled;
extern int singleScreenMode;
extern int stateNum;
extern int gameScreen;
extern bool accelPadMode;
extern int scaleMode;
extern int scaleFilter;

extern bool fpsOutput;
extern bool timeOutput;

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
void menuPrintConfig(FILE* file);

