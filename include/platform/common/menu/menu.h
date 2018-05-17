#pragma once

#include <stdio.h>

#include <vector>

#include "types.h"

struct MenuOption {
        const char* name;
        void (* function)(int);
        int numValues;
        const char* values[15];
        int defaultSelection;

        bool enabled;
        int selection;
};

struct SubMenu {
    const char* name;
    int numOptions;
    MenuOption options[13];
};

extern SubMenu menuList[];
extern const int numMenus;

extern std::vector<std::string> supportedImages;

extern bool menuOn;

extern bool customBordersEnabled;
extern int borderScaleMode;
extern int gbColorizeMode;
extern int pauseOnMenu;
extern int stateNum;
extern int gameScreen;
extern int scaleMode;
extern int scaleFilter;
extern bool fpsOutput;
extern bool timeOutput;
extern int fastForwardFrameSkip;

void setMenuDefaults();

void displayMenu();
void closeMenu();

void redrawMenu();
void updateMenu();
void printMenuMessage(const char* s);

void displaySubMenu(void (* updateFunc)());
void closeSubMenu();

int getMenuOption(const char* name);
void setMenuOption(const char* name, int value);
void enableMenuOption(const char* name);
void disableMenuOption(const char* name);

bool showConsoleDebug();

