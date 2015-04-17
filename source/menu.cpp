#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "platform/input.h"
#include "platform/system.h"
#include "cheats.h"
#include "config.h"
#include "gameboy.h"
#include "manager.h"
#include "menu.h"

const int MENU_NONE = 1;
const int MENU_3DS = 2;

const int MENU_ALL = MENU_3DS;

const int MENU_BITMASK = MENU_3DS;

void subMenuGenericUpdateFunc(); // Private function here

bool consoleDebugOutput = false;
bool menuOn = false;
int menu = 0;
int option = -1;
char printMessage[33];
int gameScreen = 0;
int singleScreenMode = 0;
int stateNum = 0;

int gbcModeOption = 0;
bool gbaModeOption = 0;
int sgbModeOption = 0;

bool soundDisabled = false;

int gbColorize = 0;

bool customBordersEnabled = false;
bool sgbBordersEnabled = false;
bool autoSavingEnabled = false;

int scaleMode = 0;
int scaleFilter = 0;

bool printerEnabled = false;

bool accelPadMode = false;

void (* subMenuUpdateFunc)();

bool fpsOutput = false;
bool timeOutput = false;

extern int halt;


// Private function used for simple submenus
void subMenuGenericUpdateFunc() {
    if(inputKeyPressed(mapMenuKey(MENU_KEY_A)) || inputKeyPressed(mapMenuKey(MENU_KEY_B)))
        closeSubMenu();
}

// Functions corresponding to menu options

void suspendFunc(int value) {
    gameboy->getAPU()->mute();
    if(!autoSavingEnabled && gameboy->getNumRamBanks()) {
        printMenuMessage("Saving SRAM...");
        mgrSave();
    }
    printMenuMessage("Saving state...");
    gameboy->saveState(-1);
    printMessage[0] = '\0';
    closeMenu();
    mgrSelectRom();
}

void exitFunc(int value) {
    gameboy->getAPU()->mute();
    if(!autoSavingEnabled && gameboy->getNumRamBanks()) {
        printMenuMessage("Saving SRAM...");
        mgrSave();
    }
    printMessage[0] = '\0';
    closeMenu();
    mgrSelectRom();
}

void exitNoSaveFunc(int value) {
    gameboy->getAPU()->mute();
    closeMenu();
    mgrSelectRom();
}

void consoleOutputFunc(int value) {
    if(value == 0) {
        fpsOutput = false;
        timeOutput = false;
        consoleDebugOutput = false;
    }
    else if(value == 1) {
        fpsOutput = false;
        timeOutput = true;
        consoleDebugOutput = false;
    }
    else if(value == 2) {
        fpsOutput = true;
        timeOutput = true;
        consoleDebugOutput = false;
    }
    else if(value == 3) {
        fpsOutput = false;
        timeOutput = false;
        consoleDebugOutput = true;
    }
}

void returnToLauncherFunc(int value) {
    systemExit();
}

void printerEnableFunc(int value) {
    if(value) {
        gameboy->getPrinter()->initGbPrinter();
    }
    printerEnabled = value;
}

void cheatFunc(int value) {
    if(!startCheatMenu())
        printMenuMessage("No cheats found!");
}

void keyConfigFunc(int value) {
    startKeyConfigChooser();
}

void saveSettingsFunc(int value) {
    printMenuMessage("Saving settings...");
    gameboy->getAPU()->mute();
    writeConfigFile();
    if(!gameboy->isGameboyPaused())
        gameboy->getAPU()->unmute();
    printMenuMessage("Settings saved.");
}

void stateSelectFunc(int value) {
    stateNum = value;
    if(gameboy->checkStateExists(stateNum)) {
        enableMenuOption("Load State");
        enableMenuOption("Delete State");
    }
    else {
        disableMenuOption("Load State");
        disableMenuOption("Delete State");
    }
}

void stateSaveFunc(int value) {
    printMenuMessage("Saving state...");
    gameboy->getAPU()->mute();
    gameboy->saveState(stateNum);
    if(!gameboy->isGameboyPaused())
        gameboy->getAPU()->unmute();
    printMenuMessage("State saved.");
    // Will activate the other state options
    stateSelectFunc(stateNum);
}

void stateLoadFunc(int value) {
    printMenuMessage("Loading state...");
    gameboy->getAPU()->mute();
    if(gameboy->loadState(stateNum) == 0) {
        closeMenu();
        systemUpdateConsole();
        printMessage[0] = '\0';
    }
}

void stateDeleteFunc(int value) {
    gameboy->getAPU()->mute();
    gameboy->deleteState(stateNum);
    // Will grey out the other state options
    stateSelectFunc(stateNum);
    if(!gameboy->isGameboyPaused())
        gameboy->getAPU()->unmute();
}

void accelPadFunc(int value) {
    accelPadMode = true;
    inputKeyRelease(mapMenuKey(MENU_KEY_A));
    closeMenu();
    systemUpdateConsole();

    printf("Exit");
}

void resetFunc(int value) {
    closeMenu();
    systemUpdateConsole();
    gameboy->init();
}

void returnFunc(int value) {
    closeMenu();
    systemUpdateConsole();
}

void gameboyModeFunc(int value) {
    gbcModeOption = value;
}

void gbaModeFunc(int value) {
    gbaModeOption = value;
}

void sgbModeFunc(int value) {
    sgbModeOption = value;
}

void setScreenFunc(int value) {
    gameScreen = value;
    systemUpdateConsole();
}

void setSingleScreenFunc(int value) {
    if(value != singleScreenMode) {
        singleScreenMode = value;
        if(singleScreenMode)
            gameboy->pause();

        if(isMenuOn()) {
            // Swap game screen
            // This will invoke systemUpdateConsole, incidentally.
            setMenuOption("Game Screen", !gameScreen);
        }
    }
}

void setScaleModeFunc(int value) {
    scaleMode = value;
    if(!isMenuOn()) {
        systemUpdateConsole();
    }

    if(value == 0) {
        gameboy->getPPU()->checkBorder();
        enableMenuOption("Console Output");
    } else {
        disableMenuOption("Console Output");
    }
}

void setScaleFilterFunc(int value) {
    scaleFilter = value;
}

void gbColorizeFunc(int value) {
    gbColorize = value;
    if(gameboy->getRomFile() != NULL) {
        gameboy->initGFXPalette();
        gameboy->getPPU()->refreshPPU();
    }
}

void customBorderEnableFunc(int value) {
    customBordersEnabled = value;
    gameboy->getPPU()->checkBorder();
}

void selectBorderFunc(int value) {
    gameboy->getPPU()->selectBorder();
}

void sgbBorderEnableFunc(int value) {
    sgbBordersEnabled = value;
    gameboy->getPPU()->checkBorder();
}

void soundEnableFunc(int value) {
    soundDisabled = !value;
}

void romInfoFunc(int value) {
    displaySubMenu(subMenuGenericUpdateFunc);
    gameboy->printRomInfo();
}

void versionInfoFunc(int value) {
    displaySubMenu(subMenuGenericUpdateFunc);
    iprintf("\x1b[2J");
    printf("GameYob %s\n", VERSION_STRING);
}

void setChanEnabled(int chan, int value) {
    if(value == 0)
        gameboy->getAPU()->disableChannel(chan);
    else
        gameboy->getAPU()->enableChannel(chan);
}

void chan1Func(int value) {
    setChanEnabled(0, value);
}

void chan2Func(int value) {
    setChanEnabled(1, value);
}

void chan3Func(int value) {
    setChanEnabled(2, value);
}

void chan4Func(int value) {
    setChanEnabled(3, value);
}

void setAutoSaveFunc(int value) {
    gameboy->getAPU()->mute();
    if(autoSavingEnabled) {
        gameboy->gameboySyncAutosave();
    } else {
        gameboy->saveGame(); // Synchronizes save file with filesystem
    }

    autoSavingEnabled = value;
    if(gameboy->isRomLoaded() && gameboy->getNumRamBanks() && !gameboy->getGBSPlayer()->gbsMode && !autoSavingEnabled) {
        enableMenuOption("Exit without saving");
    } else {
        disableMenuOption("Exit without saving");
    }

    if(!gameboy->isGameboyPaused()) {
        gameboy->getAPU()->unmute();
    }
}

struct MenuOption {
    const char* name;
    void (* function)(int);
    int numValues;
    const char* values[15];
    int defaultSelection;
    int platforms;

    bool enabled;
    int selection;
};

struct SubMenu {
    const char* name;
    int numOptions;
    MenuOption options[10];

    int selection;
};


SubMenu menuList[] = {
        {
                "ROM",
                10,
                {
                        {"Exit", exitFunc, 0, {}, 0, MENU_ALL},
                        {"Reset", resetFunc, 0, {}, 0, MENU_ALL},
                        {"State Slot", stateSelectFunc, 10, {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"}, 0, MENU_ALL},
                        {"Save State", stateSaveFunc, 0, {}, 0, MENU_ALL},
                        {"Load State", stateLoadFunc, 0, {}, 0, MENU_ALL},
                        {"Delete State", stateDeleteFunc, 0, {}, 0, MENU_ALL},
                        {"Accelerometer Pad", accelPadFunc, 0, {}, 0, MENU_ALL},
                        {"Quit to Launcher", returnToLauncherFunc, 0, {}, 0, MENU_ALL},
                        {"Exit without saving", exitNoSaveFunc, 0, {}, 0, MENU_ALL},
                        {"Suspend", suspendFunc, 0, {}, 0, MENU_ALL}
                }
        },
        {
                "Settings",
                6,
                {
                        {"Button Mapping", keyConfigFunc, 0, {}, 0, MENU_ALL},
                        {"Manage Cheats", cheatFunc, 0, {}, 0, MENU_ALL},
                        {"Console Output", consoleOutputFunc, 4, {"Off", "Time", "FPS+Time", "Debug"}, 0, MENU_ALL},
                        {"GB Printer", printerEnableFunc, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Autosaving", setAutoSaveFunc, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Save Settings", saveSettingsFunc, 0, {}, 0, MENU_ALL}
                }
        },
        {
                "Display",
                7,
                {
                        {"Game Screen", setScreenFunc, 2, {"Top", "Bottom"}, 0, MENU_ALL},
                        {"Single Screen", setSingleScreenFunc, 2, {"Off", "On"}, 0, MENU_ALL},
                        {"Scaling", setScaleModeFunc, 3, {"Off", "Aspect", "Full"}, 0, MENU_ALL},
                        {"Scale Filter", setScaleFilterFunc, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Colorize GB", gbColorizeFunc, 13, {"Off", "Auto", "Inverted", "Pastel Mix", "Red", "Orange", "Yellow", "Green", "Blue", "Brown", "Dark Green", "Dark Blue", "Dark Brown"}, 1, MENU_ALL},
                        {"SGB Borders", sgbBorderEnableFunc, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Custom Border", customBorderEnableFunc, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Select Border", (void (*)(int)) selectBorderFunc, 0, {}, 0, MENU_ALL},
                }
        },
        {
                "GB Modes",
                3,
                {
                        {"Detect GBA", gbaModeFunc, 2, {"Off", "On"}, 0, MENU_ALL},
                        {"GBC Mode", gameboyModeFunc, 3, {"Off", "If Needed", "On"}, 2, MENU_ALL},
                        {"SGB Mode", sgbModeFunc, 3, {"Off", "Prefer GBC", "Prefer SGB"}, 0, MENU_ALL}
                }
        },
        {
                "Debug",
                3,
                {
                        {"Sound", soundEnableFunc, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"ROM Info", romInfoFunc, 0, {}, 0, MENU_ALL},
                        {"Version Info", versionInfoFunc, 0, {}, 0, MENU_ALL}
                }
        },
        {
                "Sound Channels",
                4,
                {
                        {"Channel 1", chan1Func, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Channel 2", chan2Func, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Channel 3", chan3Func, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Channel 4", chan4Func, 2, {"Off", "On"}, 1, MENU_ALL}
                }
        }
};
const int numMenus = sizeof(menuList) / sizeof(SubMenu);

void setMenuDefaults() {
    for(int i = 0; i < numMenus; i++) {
        menuList[i].selection = -1;
        for(int j = 0; j < menuList[i].numOptions; j++) {
            menuList[i].options[j].selection = menuList[i].options[j].defaultSelection;
            menuList[i].options[j].enabled = true;
            if(menuList[i].options[j].numValues != 0 && menuList[i].options[j].platforms & MENU_BITMASK) {
                int selection = menuList[i].options[j].defaultSelection;
                menuList[i].options[j].function(selection);
            }
        }
    }
}

void displayMenu() {
    menuOn = true;
    systemUpdateConsole();
    redrawMenu();
}

void closeMenu() {
    menuOn = false;
    iprintf("\x1b[2J");
    gameboy->unpause();
}

bool isMenuOn() {
    return menuOn;
}

// Some helper functions
void menuCursorUp() {
    option--;
    if(option == -1) {
        return;
    }

    if(option < -1) {
        option = menuList[menu].numOptions - 1;
    }

    if(!(menuList[menu].options[option].platforms & MENU_BITMASK)) {
        menuCursorUp();
    }
}

void menuCursorDown() {
    option++;
    if(option >= menuList[menu].numOptions) {
        option = -1;
    } else if(!(menuList[menu].options[option].platforms & MENU_BITMASK)) {
        menuCursorDown();
    }
}

// Get the number of rows down the selected option is
// Necessary because of leaving out certain options in certain platforms
int menuGetOptionRow() {
    if(option == -1) {
        return option;
    }

    int row = 0;
    for(int i = 0; i < option; i++) {
        if(menuList[menu].options[i].platforms & MENU_BITMASK) {
            row++;
        }
    }

    return row;
}

void menuSetOptionRow(int row) {
    if(row == -1) {
        option = -1;
        return;
    }
    row++;
    int lastValidRow = -1;
    for(int i = 0; i < menuList[menu].numOptions; i++) {
        if(menuList[menu].options[i].platforms & MENU_BITMASK) {
            row--;
            lastValidRow = i;
        }

        if(row == 0) {
            option = i;
            return;
        }
    }
    // Too high
    option = lastValidRow;
}

// Get the number of VISIBLE rows for this platform
int menuGetNumRows() {
    int count = 0;
    for(int i = 0; i < menuList[menu].numOptions; i++) {
        if(menuList[menu].options[i].platforms & MENU_BITMASK) {
            count++;
        }
    }

    return count;
}

void redrawMenu() {
    iprintf("\x1b[2J");

    int width = systemGetConsoleWidth();
    int height = systemGetConsoleHeight();

    // Top line: submenu
    int pos = 0;
    int nameStart = (width - strlen(menuList[menu].name) - 2) / 2;
    if(option == -1) {
        nameStart -= 2;
        printf("\x1b[1m\x1b[32m<\x1b[0m");
    } else {
        printf("<");
    }

    pos++;
    for(; pos < nameStart; pos++) {
        printf(" ");
    }

    if(option == -1) {
        printf("\x1b[1m\x1b[33m* \x1b[0m");
        pos += 2;
    }

    std::string color = (option == -1 ? "\x1b[1m\x1b[33m" : "");
    printf((color + "[%s]\x1b[0m").c_str(), menuList[menu].name);
    pos += 2 + strlen(menuList[menu].name);
    if(option == -1) {
        printf("\x1b[1m\x1b[33m *\x1b[0m");
        pos += 2;
    }

    for(; pos < width - 1; pos++) {
        printf(" ");
    }

    if(option == -1) {
        printf("\x1b[1m\x1b[32m>\x1b[0m");
    } else {
        printf(">");
    }

    printf("\n");

    // Rest of the lines: options
    for(int i = 0; i < menuList[menu].numOptions; i++) {
        if(!(menuList[menu].options[i].platforms & MENU_BITMASK)) {
            continue;
        }

        std::string option_color;
        if(!menuList[menu].options[i].enabled) {
            option_color = "\x1b[2m";
        } else if(option == i) {
            option_color = "\x1b[1m\x1b[33m";
        } else {
            option_color = "";
        }

        if(menuList[menu].options[i].numValues == 0) {
            for(unsigned int j = 0; j < (width - strlen(menuList[menu].options[i].name)) / 2 - 2; j++) {
                printf(" ");
            }

            if(i == option) {
                printf((option_color + "* %s *\n\n\x1b[0m").c_str(), menuList[menu].options[i].name);
            } else {
                printf((option_color + "  %s  \n\n\x1b[0m").c_str(), menuList[menu].options[i].name);
            }
        } else {
            for(unsigned int j = 0; j < width / 2 - strlen(menuList[menu].options[i].name); j++) {
                printf(" ");
            }

            if(i == option) {
                printf((option_color + "* \x1b[0m").c_str());
                printf((option_color + "%s  \x1b[0m").c_str(), menuList[menu].options[i].name);
                printf(((menuList[menu].options[i].enabled ? "\x1b[1m\x1b[32m" : option_color) + "%s\x1b[0m").c_str(), menuList[menu].options[i].values[menuList[menu].options[i].selection]);
                printf((option_color + " *\x1b[0m").c_str());
            } else {
                printf("  ");
                printf((option_color + "%s  \x1b[0m").c_str(), menuList[menu].options[i].name);
                printf((option_color + "%s\x1b[0m").c_str(), menuList[menu].options[i].values[menuList[menu].options[i].selection]);
            }

            printf("\n\n");
        }
    }

    // Message at the bottom
    if(printMessage[0] != '\0') {
        int rows = menuGetNumRows();
        int newlines = height - 1 - (rows * 2 + 2) - 1;
        for(int i = 0; i < newlines; i++)
            printf("\n");
        int spaces = width - 1 - strlen(printMessage);
        for(int i = 0; i < spaces; i++)
            printf(" ");
        printf("%s\n", printMessage);

        printMessage[0] = '\0';
    }
}

// Called each vblank while the menu is on
void updateMenu() {
    if(!isMenuOn())
        return;

    if(subMenuUpdateFunc != 0) {
        subMenuUpdateFunc();
        return;
    }

    bool redraw = false;
    // Get input
    if(inputKeyRepeat(mapMenuKey(MENU_KEY_UP))) {
        menuCursorUp();
        redraw = true;
    }
    else if(inputKeyRepeat(mapMenuKey(MENU_KEY_DOWN))) {
        menuCursorDown();
        redraw = true;
    }
    else if(inputKeyRepeat(mapMenuKey(MENU_KEY_LEFT))) {
        if(option == -1) {
            menu--;
            if(menu < 0)
                menu = numMenus - 1;
        }
        else if(menuList[menu].options[option].numValues != 0 && menuList[menu].options[option].enabled) {
            int selection = menuList[menu].options[option].selection - 1;
            if(selection < 0)
                selection = menuList[menu].options[option].numValues - 1;
            menuList[menu].options[option].selection = selection;
            menuList[menu].options[option].function(selection);
        }
        redraw = true;
    }
    else if(inputKeyRepeat(mapMenuKey(MENU_KEY_RIGHT))) {
        if(option == -1) {
            menu++;
            if(menu >= numMenus)
                menu = 0;
        }
        else if(menuList[menu].options[option].numValues != 0 && menuList[menu].options[option].enabled) {
            int selection = menuList[menu].options[option].selection + 1;
            if(selection >= menuList[menu].options[option].numValues)
                selection = 0;
            menuList[menu].options[option].selection = selection;
            menuList[menu].options[option].function(selection);
        }
        redraw = true;
    }
    else if(inputKeyPressed(mapMenuKey(MENU_KEY_A))) {
        inputKeyRelease(mapMenuKey(MENU_KEY_A));
        if(option >= 0 && menuList[menu].options[option].numValues == 0 && menuList[menu].options[option].enabled) {
            menuList[menu].options[option].function(menuList[menu].options[option].selection);
        }
        redraw = true;
    }
    else if(inputKeyPressed(mapMenuKey(MENU_KEY_B))) {
        inputKeyRelease(mapMenuKey(MENU_KEY_B));
        closeMenu();
        systemUpdateConsole();
    }
    else if(inputKeyPressed(mapMenuKey(MENU_KEY_L))) {
        int row = menuGetOptionRow();
        menu--;
        if(menu < 0)
            menu = numMenus - 1;
        menuSetOptionRow(row);
        redraw = true;
    } else if(inputKeyPressed(mapMenuKey(MENU_KEY_R))) {
        int row = menuGetOptionRow();
        menu++;
        if(menu >= numMenus)
            menu = 0;
        menuSetOptionRow(row);
        redraw = true;
    }

    if(redraw && subMenuUpdateFunc == 0 && isMenuOn()) {// The menu may have been closed by an option
        redrawMenu();
    }
}

// Message will be printed immediately, but also stored in case it's overwritten 
// right away.
void printMenuMessage(const char* s) {
    int width = systemGetConsoleWidth();
    int height = systemGetConsoleHeight();
    int rows = menuGetNumRows();

    bool hadPreviousMessage = printMessage[0] != '\0';
    strncpy(printMessage, s, 33);

    if(hadPreviousMessage) {
        printf("\r");
    }
    else {
        int newlines = height - 1 - (rows * 2 + 2) - 1;
        for(int i = 0; i < newlines; i++)
            printf("\n");
    }
    int spaces = width - 1 - strlen(printMessage);
    for(int i = 0; i < spaces; i++)
        printf(" ");
    printf("%s", printMessage);

    fflush(stdout);
}

void displaySubMenu(void (* updateFunc)()) {
    subMenuUpdateFunc = updateFunc;
}

void closeSubMenu() {
    subMenuUpdateFunc = NULL;
    redrawMenu();
}


int getMenuOption(const char* optionName) {
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                return menuList[i].options[j].selection;
            }
        }
    }
    return 0;
}

void setMenuOption(const char* optionName, int value) {
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                if(!(menuList[i].options[j].platforms & MENU_BITMASK)) {
                    continue;
                }

                menuList[i].options[j].selection = value;
                menuList[i].options[j].function(value);
                return;
            }
        }
    }
}

void enableMenuOption(const char* optionName) {
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                menuList[i].options[j].enabled = true;
                return;
            }
        }
    }
}

void disableMenuOption(const char* optionName) {
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                menuList[i].options[j].enabled = false;
                return;
            }
        }
    }
}

void menuParseConfig(char* line) {
    char* equalsPos = strchr(line, '=');
    if(equalsPos == 0)
        return;
    *equalsPos = '\0';
    const char* option = line;
    const char* value = equalsPos + 1;
    int val = atoi(value);
    setMenuOption(option, val);
}

void menuPrintConfig(FILE* file) {
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(menuList[i].options[j].platforms & MENU_BITMASK && menuList[i].options[j].numValues != 0) {
                fprintf(file, "%s=%d\n", menuList[i].options[j].name, menuList[i].options[j].selection);
            }
        }
    }
}

