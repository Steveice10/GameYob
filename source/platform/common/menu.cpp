#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sstream>

#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#include "gb_apu/Gb_Apu.h"
#include "platform/common/cheats.h"
#include "platform/common/config.h"
#include "platform/common/filechooser.h"
#include "platform/common/gbsplayer.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"
#include "cheatengine.h"
#include "gameboy.h"
#include "ppu.h"
#include "printer.h"
#include "romfile.h"

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
int pauseOnMenu = 0;
int stateNum = 0;

int borderSetting = 0;

int scaleMode = 0;
int scaleFilter = 0;
int borderScaleMode = 0;

bool accelPadMode = false;

void (* subMenuUpdateFunc)();

bool fpsOutput = false;
bool timeOutput = false;

FileChooser borderChooser("/", {"png", "bmp"}, true);
FileChooser biosChooser("/", {"bin"}, true);

// Private function used for simple submenus
void subMenuGenericUpdateFunc() {
    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if(key == UI_KEY_A || key == UI_KEY_B) {
            closeSubMenu();
            return;
        }
    }
}

// Functions corresponding to menu options

void suspendFunc(int value) {
    if(!gameboy->autosaveEnabled && gameboy->isRomLoaded() && gameboy->getRomFile()->getRamBanks() > 0) {
        printMenuMessage("Saving SRAM...");
        mgrSave();
    }

    printMenuMessage("Saving state...");
    mgrSaveState(-1);
    printMessage[0] = '\0';
    closeMenu();
    mgrSelectRom();
}

void exitFunc(int value) {
    if(!gameboy->autosaveEnabled && gameboy->isRomLoaded() && gameboy->getRomFile()->getRamBanks() > 0) {
        printMenuMessage("Saving SRAM...");
        mgrSave();
    }

    printMessage[0] = '\0';
    closeMenu();
    mgrSelectRom();
}

void exitNoSaveFunc(int value) {
    closeMenu();
    mgrSelectRom();
}

void consoleOutputFunc(int value) {
    if(value == 0) {
        fpsOutput = false;
        timeOutput = false;
        consoleDebugOutput = false;
    } else if(value == 1) {
        fpsOutput = false;
        timeOutput = true;
        consoleDebugOutput = false;
    } else if(value == 2) {
        fpsOutput = true;
        timeOutput = true;
        consoleDebugOutput = false;
    } else if(value == 3) {
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

    gameboy->printerEnabled = (bool) value;
}

void cheatFunc(int value) {
    if(gameboy->getCheatEngine() != NULL && gameboy->getCheatEngine()->getNumCheats() != 0) {
        startCheatMenu();
    } else {
        printMenuMessage("No cheats found!");
    }
}

void keyConfigFunc(int value) {
    startKeyConfigChooser();
}

void saveSettingsFunc(int value) {
    printMenuMessage("Saving settings...");
    writeConfigFile();
    printMenuMessage("Settings saved.");
}

void stateSelectFunc(int value) {
    stateNum = value;
    if(mgrStateExists(stateNum)) {
        enableMenuOption("Load State");
        enableMenuOption("Delete State");
    } else {
        disableMenuOption("Load State");
        disableMenuOption("Delete State");
    }
}

void stateSaveFunc(int value) {
    printMenuMessage("Saving state...");
    if(mgrSaveState(stateNum)) {
        printMenuMessage("State saved.");
    } else {
        printMenuMessage("Could not save state.");
    }

    // Will activate the other state options
    stateSelectFunc(stateNum);
}

void stateLoadFunc(int value) {
    if(!mgrStateExists(stateNum)) {
        printMenuMessage("State does not exist.");
    }

    printMenuMessage("Loading state...");
    if(mgrLoadState(stateNum)) {
        closeMenu();
        printMessage[0] = '\0';
    } else {
        printMenuMessage("Could not load state.");
    }
}

void stateDeleteFunc(int value) {
    mgrDeleteState(stateNum);

    // Will grey out the other state options
    stateSelectFunc(stateNum);
}

void accelPadFunc(int value) {
    accelPadMode = true;
    closeMenu();

    uiPrint("Exit");
    uiFlush();
}

void resetFunc(int value) {
    closeMenu();
    gameboy->init();

    if(gameboy->isRomLoaded() && gameboy->getRomFile()->isGBS()) {
        gbsPlayerReset();
        gbsPlayerDraw();
    }
}

void returnFunc(int value) {
    closeMenu();
}

void gameboyModeFunc(int value) {
    gameboy->gbcModeOption = value;
}

void gbaModeFunc(int value) {
    gameboy->gbaModeOption = (bool) value;
}

void sgbModeFunc(int value) {
    gameboy->sgbModeOption = value;
}

void biosEnableFunc(int value) {
    gameboy->biosMode = value;
}

void selectGbBiosFunc(int value) {
    char* filename = biosChooser.startFileChooser();
    if(filename != NULL) {
        gbBiosPath = filename;
        free(filename);

        mgrRefreshBios();
    }
}

void selectGbcBiosFunc(int value) {
    char* filename = biosChooser.startFileChooser();
    if(filename != NULL) {
        gbcBiosPath = filename;
        free(filename);

        mgrRefreshBios();
    }
}

void setScreenFunc(int value) {
    gameScreen = value;
    uiUpdateScreen();
}

void setPauseOnMenuFunc(int value) {
    if(value != pauseOnMenu) {
        pauseOnMenu = value;
        if(pauseOnMenu) {
            gameboy->pause();
        } else {
            gameboy->unpause();
        }
    }
}

void setScaleModeFunc(int value) {
    scaleMode = value;

    mgrRefreshBorder();
}

void setScaleFilterFunc(int value) {
    scaleFilter = value;
}

void setBorderScaleModeFunc(int value) {
    borderScaleMode = value;

    mgrRefreshBorder();
}

void setFastForwardFrameSkipFunc(int value) {
    gameboy->getPPU()->fastForwardFrameSkip = value;
}

void gbColorizeFunc(int value) {
    gameboy->gbColorizeMode = value;
    if(gameboy->isRomLoaded() && gameboy->gbMode == GB) {
        gameboy->refreshGFXPalette();
    }
}

void selectBorderFunc(int value) {
    char* filename = borderChooser.startFileChooser();
    if(filename != NULL) {
        borderPath = filename;
        free(filename);

        mgrRefreshBorder();
    }
}

void borderFunc(int value) {
    borderSetting = value;
    if(borderSetting == 1) {
        enableMenuOption("Select Border");
    } else {
        disableMenuOption("Select Border");
    }

    mgrRefreshBorder();
}

void soundEnableFunc(int value) {
    gameboy->soundEnabled = (bool) value;
}

void romInfoFunc(int value) {
    if(gameboy->isRomLoaded()) {
        displaySubMenu(subMenuGenericUpdateFunc);

        static const char* mbcNames[] = {"ROM", "MBC1", "MBC2", "MBC3", "MBC5", "MBC7", "MMM01", "HUC1", "HUC3", "CAMERA", "TAMA5"};

        uiClear();
        uiPrint("ROM Title: \"%s\"\n", gameboy->getRomFile()->getRomTitle().c_str());
        uiPrint("CGB: Supported: %d, Required: %d\n", gameboy->getRomFile()->isCgbSupported(), gameboy->getRomFile()->isCgbRequired());
        uiPrint("Cartridge type: %.2x (%s)\n", gameboy->getRomFile()->getRawMBC(), mbcNames[gameboy->getRomFile()->getMBC()]);
        uiPrint("ROM Size: %.2x (%d banks)\n", gameboy->getRomFile()->getRawRomSize(), gameboy->getRomFile()->getRomBanks());
        uiPrint("RAM Size: %.2x (%d banks)\n", gameboy->getRomFile()->getRawRamSize(), gameboy->getRomFile()->getRamBanks());
        uiFlush();
    }
}

void versionInfoFunc(int value) {
    displaySubMenu(subMenuGenericUpdateFunc);

    uiClear();
    uiPrint("Version: %s\n", VERSION_STRING);
    uiFlush();
}

void setChanEnabled(int chan, int value) {
    gameboy->getAPU()->set_osc_enabled(chan, value == 1);
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
    bool prev = gameboy->autosaveEnabled;
    gameboy->autosaveEnabled = (bool) value;

    if(gameboy->isRomLoaded()) {
        if(prev) {
            gameboy->gameboySyncAutosave();
        } else {
            gameboy->saveGame(); // Synchronizes save file with filesystem
        }

        if(!gameboy->autosaveEnabled && gameboy->getRomFile()->getRamBanks() > 0 && !gameboy->getRomFile()->isGBS()) {
            enableMenuOption("Exit without saving");
        } else {
            disableMenuOption("Exit without saving");
        }
    }
}

int listenSocket = -1;
FILE* linkSocket = NULL;
std::string linkIp = "";

void listenUpdateFunc() {
    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if(key == UI_KEY_A || key == UI_KEY_B) {
            if(listenSocket != -1) {
                close(listenSocket);
                listenSocket = -1;
            }

            closeSubMenu();
            return;
        }
    }

    if(listenSocket != -1 && linkSocket == NULL) {
        linkSocket = systemSocketAccept(listenSocket, &linkIp);
        if(linkSocket != NULL) {
            close(listenSocket);
            listenSocket = -1;

            uiClear();
            uiPrint("Connected to %s.\n", linkIp.c_str());
            uiPrint("Press A or B to continue.\n");
            uiFlush();
        }
    }
}

void listenFunc(int value) {
    displaySubMenu(listenUpdateFunc);

    uiClear();

    if(linkSocket != NULL) {
        uiPrint("Already connected.\n");
        uiPrint("Press A or B to continue.\n");
    } else {
        listenSocket = systemSocketListen(5000);
        if(listenSocket >= 0) {
            uiPrint("Listening for connection...\n");
            uiPrint("Local IP: %s\n", systemGetIP().c_str());
            uiPrint("Press A or B to cancel.\n");
        } else {
            uiPrint("Failed to open socket: %s\n", strerror(errno));
            uiPrint("Press A or B to continue.\n");
        }
    }

    uiFlush();
}

bool connectPerformed = false;
std::string connectIp = "000.000.000.000";
u32 connectSelection = 0;

void drawConnectSelector() {
    uiClear();
    uiPrint("Input IP to connect to:\n");
    uiPrint("%s\n", connectIp.c_str());
    for(u32 i = 0; i < connectSelection; i++) {
        uiPrint(" ");
    }

    uiPrint("^\n");
    uiPrint("Press A to confirm, B to cancel.\n");
    uiFlush();
}

void connectUpdateFunc() {
    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if((connectPerformed && key == UI_KEY_A) || key == UI_KEY_B) {
            connectPerformed = false;
            connectIp = "000.000.000.000";
            connectSelection = 0;

            closeSubMenu();
            return;
        }

        if(!connectPerformed) {
            if(key == UI_KEY_A) {
                std::string trimmedIp = connectIp;

                bool removeZeros = true;
                for(std::string::size_type i = 0; i < trimmedIp.length(); i++) {
                    if(removeZeros && trimmedIp[i] == '0' && i != trimmedIp.length() - 1 && trimmedIp[i + 1] != '.') {
                        trimmedIp.erase(i, 1);
                        i--;
                    } else {
                        removeZeros = trimmedIp[i] == '.';
                    }
                }

                connectPerformed = true;

                uiClear();
                uiPrint("Connecting to %s...\n", trimmedIp.c_str());

                linkSocket = systemSocketConnect(trimmedIp, 5000);
                if(linkSocket != NULL) {
                    linkIp = trimmedIp;;
                    uiPrint("Connected to %s.\n", linkIp.c_str());
                } else {
                    uiPrint("Failed to connect to socket: %s\n", strerror(errno));
                }

                uiPrint("Press A or B to continue.\n");
                uiFlush();
            }

            bool redraw = false;
            if(key == UI_KEY_LEFT && connectSelection > 0) {
                connectSelection--;
                if(connectIp[connectSelection] == '.') {
                    connectSelection--;
                }

                redraw = true;
            }

            if(key == UI_KEY_RIGHT && connectSelection < connectIp.length() - 1) {
                connectSelection++;
                if(connectIp[connectSelection] == '.') {
                    connectSelection++;
                }

                redraw = true;
            }

            if(key == UI_KEY_UP) {
                connectIp[connectSelection]++;
                if(connectIp[connectSelection] > '9') {
                    connectIp[connectSelection] = '0';
                }

                redraw = true;
            }

            if(key == UI_KEY_DOWN) {
                connectIp[connectSelection]--;
                if(connectIp[connectSelection] < '0') {
                    connectIp[connectSelection] = '9';
                }

                redraw = true;
            }

            if(redraw) {
                drawConnectSelector();
            }
        }
    }
}

void connectFunc(int value) {
    displaySubMenu(connectUpdateFunc);
    if(linkSocket != NULL) {
        connectPerformed = true;

        uiClear();
        uiPrint("Already connected.\n");
        uiPrint("Press A or B to continue.\n");
        uiFlush();
    } else {
        drawConnectSelector();
    }
}

void disconnectFunc(int value) {
    displaySubMenu(subMenuGenericUpdateFunc);

    uiClear();

    if(linkSocket != NULL) {
        fclose(linkSocket);
        linkSocket = NULL;
        linkIp = "";

        uiPrint("Disconnected.\n");
    } else {
        uiPrint("Not connected.\n");
    }

    uiPrint("Press A or B to continue.\n");

    uiFlush();
}

void connectionInfoFunc(int value) {
    displaySubMenu(subMenuGenericUpdateFunc);

    uiClear();

    uiPrint("Status: ");
    if(linkSocket != NULL) {
        uiSetTextColor(TEXT_COLOR_GREEN);
        uiPrint("Connected");
        uiSetTextColor(TEXT_COLOR_NONE);
    } else {
        uiSetTextColor(TEXT_COLOR_RED);
        uiPrint("Disconnected");
        uiSetTextColor(TEXT_COLOR_NONE);
    }

    uiPrint("\n");
    uiPrint("IP: %s\n", linkIp.c_str());
    uiPrint("\n");
    uiPrint("Press A or B to continue.\n");
    uiFlush();
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
    MenuOption options[13];

    int selection;
};

SubMenu menuList[] = {
        {
                "Game",
                13,
                {
                        {"Exit", exitFunc, 0, {}, 0, MENU_ALL},
                        {"Reset", resetFunc, 0, {}, 0, MENU_ALL},
                        {"Suspend", suspendFunc, 0, {}, 0, MENU_ALL},
                        {"ROM Info", romInfoFunc, 0, {}, 0, MENU_ALL},
                        {"Version Info", versionInfoFunc, 0, {}, 0, MENU_ALL},
                        {"State Slot", stateSelectFunc, 10, {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"}, 0, MENU_ALL},
                        {"Save State", stateSaveFunc, 0, {}, 0, MENU_ALL},
                        {"Load State", stateLoadFunc, 0, {}, 0, MENU_ALL},
                        {"Delete State", stateDeleteFunc, 0, {}, 0, MENU_ALL},
                        {"Manage Cheats", cheatFunc, 0, {}, 0, MENU_ALL},
                        {"Accelerometer Pad", accelPadFunc, 0, {}, 0, MENU_ALL},
                        {"Exit without saving", exitNoSaveFunc, 0, {}, 0, MENU_ALL},
                        {"Quit to Launcher", returnToLauncherFunc, 0, {}, 0, MENU_ALL}
                }
        },
        {
                "GameYob",
                5,
                {
                        {"Button Mapping", keyConfigFunc, 0, {}, 0, MENU_ALL},
                        {"Console Output", consoleOutputFunc, 4, {"Off", "Time", "FPS+Time", "Debug"}, 0, MENU_ALL},
                        {"Autosaving", setAutoSaveFunc, 2, {"Off", "On"}, 0, MENU_ALL},
                        {"Pause on Menu", setPauseOnMenuFunc, 2, {"Off", "On"}, 0, MENU_ALL},
                        {"Save Settings", saveSettingsFunc, 0, {}, 0, MENU_ALL}
                }
        },
        {
                "Gameboy",
                7,
                {
                        {"GB Printer", printerEnableFunc, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"GBA Mode", gbaModeFunc, 2, {"Off", "On"}, 0, MENU_ALL},
                        {"GBC Mode", gameboyModeFunc, 3, {"Off", "If Needed", "On"}, 2, MENU_ALL},
                        {"SGB Mode", sgbModeFunc, 3, {"Off", "Prefer GBC", "Prefer SGB"}, 1, MENU_ALL},
                        {"BIOS Mode", biosEnableFunc, 4, {"Off", "Auto", "GB", "GBC"}, 1, MENU_ALL},
                        {"Select GB BIOS", selectGbBiosFunc, 0, {}, 0, MENU_ALL},
                        {"Select GBC BIOS", selectGbcBiosFunc, 0, {}, 0, MENU_ALL}
                }
        },
        {
                "Display",
                8,
                {
                        {"Game Screen", setScreenFunc, 2, {"Top", "Bottom"}, 0, MENU_ALL},
                        {"Scaling", setScaleModeFunc, 5, {"Off", "125%", "150%", "Aspect", "Full"}, 0, MENU_ALL},
                        {"Scale Filter", setScaleFilterFunc, 3, {"Nearest", "Linear", "Scale2x"}, 1, MENU_ALL},
                        {"FF Frame Skip", setFastForwardFrameSkipFunc, 4, {"0", "1", "2", "3"}, 3, MENU_ALL},
                        {"Colorize GB", gbColorizeFunc, 14, {"Off", "Auto", "Inverted", "Pastel Mix", "Red", "Orange", "Yellow", "Green", "Blue", "Brown", "Dark Green", "Dark Blue", "Dark Brown", "Classic Green"}, 1, MENU_ALL},
                        {"Borders", borderFunc, 3, {"Off", "Custom", "SGB"}, 1, MENU_ALL},
                        {"Border Scaling", setBorderScaleModeFunc, 2, {"Pre-Scaled", "Scale Base"}, 0, MENU_ALL},
                        {"Select Border", selectBorderFunc, 0, {}, 0, MENU_ALL}
                }
        },
        {
                "Sound",
                5,
                {
                        {"Master", soundEnableFunc, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Channel 1", chan1Func, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Channel 2", chan2Func, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Channel 3", chan3Func, 2, {"Off", "On"}, 1, MENU_ALL},
                        {"Channel 4", chan4Func, 2, {"Off", "On"}, 1, MENU_ALL}
                }
        },
        /*{
                "Link",
                4,
                {
                        {"Listen", listenFunc, 0, {}, 0, MENU_ALL},
                        {"Connect", connectFunc, 0, {}, 0, MENU_ALL},
                        {"Disconnect", disconnectFunc, 0, {}, 0, MENU_ALL},
                        {"Connection Info", connectionInfoFunc, 0, {}, 0, MENU_ALL}
                }
        },*/
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
    inputReleaseAll();
    menuOn = true;
    redrawMenu();
}

void closeMenu() {
    inputReleaseAll();
    menuOn = false;

    uiClear();
    uiFlush();

    gameboy->unpause();

    if(gameboy->isRomLoaded() && gameboy->getRomFile()->isGBS()) {
        gbsPlayerDraw();
    }
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
    uiClear();

    int width = uiGetWidth();
    int height = uiGetHeight();

    // Top line: submenu
    int pos = 0;
    int nameStart = (width - strlen(menuList[menu].name) - 2) / 2;
    if(option == -1) {
        nameStart -= 2;

        uiSetTextColor(TEXT_COLOR_GREEN);
        uiPrint("<");
        uiSetTextColor(TEXT_COLOR_NONE);
    } else {
        uiPrint("<");
    }

    pos++;
    for(; pos < nameStart; pos++) {
        uiPrint(" ");
    }

    if(option == -1) {
        uiSetTextColor(TEXT_COLOR_YELLOW);
        uiPrint("* ");
        uiSetTextColor(TEXT_COLOR_NONE);

        pos += 2;
    }

    if(option == -1) {
        uiSetTextColor(TEXT_COLOR_YELLOW);
    }

    uiPrint("[%s]", menuList[menu].name);
    uiSetTextColor(TEXT_COLOR_NONE);

    pos += 2 + strlen(menuList[menu].name);
    if(option == -1) {
        uiSetTextColor(TEXT_COLOR_YELLOW);
        uiPrint(" *");
        uiSetTextColor(TEXT_COLOR_NONE);

        pos += 2;
    }

    for(; pos < width - 1; pos++) {
        uiPrint(" ");
    }

    if(option == -1) {
        uiSetTextColor(TEXT_COLOR_GREEN);
        uiPrint(">");
        uiSetTextColor(TEXT_COLOR_NONE);
    } else {
        uiPrint(">");
    }

    uiPrint("\n");

    // Rest of the lines: options
    for(int i = 0; i < menuList[menu].numOptions; i++) {
        if(!(menuList[menu].options[i].platforms & MENU_BITMASK)) {
            continue;
        }

        if(!menuList[menu].options[i].enabled) {
            uiSetTextColor(TEXT_COLOR_GRAY);
        } else if(option == i) {
            uiSetTextColor(TEXT_COLOR_YELLOW);
        }

        if(menuList[menu].options[i].numValues == 0) {
            for(unsigned int j = 0; j < (width - strlen(menuList[menu].options[i].name)) / 2 - 2; j++) {
                uiPrint(" ");
            }

            if(i == option) {
                uiPrint("* %s *\n", menuList[menu].options[i].name);
            } else {
                uiPrint("  %s  \n", menuList[menu].options[i].name);
            }

            uiPrint("\n");
        } else {
            for(unsigned int j = 0; j < width / 2 - strlen(menuList[menu].options[i].name); j++) {
                uiPrint(" ");
            }

            if(i == option) {
                uiPrint("* ");
                uiPrint("%s  ", menuList[menu].options[i].name);

                if(menuList[menu].options[i].enabled) {
                    uiSetTextColor(TEXT_COLOR_GREEN);
                }

                uiPrint("%s", menuList[menu].options[i].values[menuList[menu].options[i].selection]);

                if(!menuList[menu].options[i].enabled) {
                    uiSetTextColor(TEXT_COLOR_GRAY);
                } else if(option == i) {
                    uiSetTextColor(TEXT_COLOR_YELLOW);
                }

                uiPrint(" *");
            } else {
                uiPrint("  ");
                uiPrint("%s  ", menuList[menu].options[i].name);
                uiPrint("%s", menuList[menu].options[i].values[menuList[menu].options[i].selection]);
            }

            uiPrint("\n\n");
        }

        uiSetTextColor(TEXT_COLOR_NONE);
    }

    // Message at the bottom
    if(printMessage[0] != '\0') {
        int rows = menuGetNumRows();
        int newlines = height - 1 - (rows * 2 + 2) - 1;
        for(int i = 0; i < newlines; i++) {
            uiPrint("\n");
        }

        int spaces = width - 1 - strlen(printMessage);
        for(int i = 0; i < spaces; i++) {
            uiPrint(" ");
        }

        uiPrint("%s\n", printMessage);

        printMessage[0] = '\0';
    }

    uiFlush();
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
    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if(key == UI_KEY_UP) {
            menuCursorUp();
            redraw = true;
        } else if(key == UI_KEY_DOWN) {
            menuCursorDown();
            redraw = true;
        } else if(key == UI_KEY_LEFT) {
            if(option == -1) {
                menu--;
                if(menu < 0) {
                    menu = numMenus - 1;
                }
            } else if(menuList[menu].options[option].numValues != 0 && menuList[menu].options[option].enabled) {
                int selection = menuList[menu].options[option].selection - 1;
                if(selection < 0) {
                    selection = menuList[menu].options[option].numValues - 1;
                }

                menuList[menu].options[option].selection = selection;
                menuList[menu].options[option].function(selection);
            }

            redraw = true;
        } else if(key == UI_KEY_RIGHT) {
            if(option == -1) {
                menu++;
                if(menu >= numMenus) {
                    menu = 0;
                }
            } else if(menuList[menu].options[option].numValues != 0 && menuList[menu].options[option].enabled) {
                int selection = menuList[menu].options[option].selection + 1;
                if(selection >= menuList[menu].options[option].numValues) {
                    selection = 0;
                }

                menuList[menu].options[option].selection = selection;
                menuList[menu].options[option].function(selection);
            }
            redraw = true;
        } else if(key == UI_KEY_A) {
            if(option >= 0 && menuList[menu].options[option].numValues == 0 && menuList[menu].options[option].enabled) {
                menuList[menu].options[option].function(menuList[menu].options[option].selection);
            }

            redraw = true;
        } else if(key == UI_KEY_B) {
            closeMenu();
        } else if(key == UI_KEY_L) {
            int row = menuGetOptionRow();
            menu--;
            if(menu < 0) {
                menu = numMenus - 1;
            }

            menuSetOptionRow(row);
            redraw = true;
        } else if(key == UI_KEY_R) {
            int row = menuGetOptionRow();
            menu++;
            if(menu >= numMenus) {
                menu = 0;
            }

            menuSetOptionRow(row);
            redraw = true;
        }
    }

    if(redraw && subMenuUpdateFunc == 0 && isMenuOn()) {// The menu may have been closed by an option
        redrawMenu();
    }
}

// Message will be printed immediately, but also stored in case it's overwritten
// right away.
void printMenuMessage(const char* s) {
    int width = uiGetWidth();
    int height = uiGetHeight();
    int rows = menuGetNumRows();

    bool hadPreviousMessage = printMessage[0] != '\0';
    strncpy(printMessage, s, 33);

    if(hadPreviousMessage) {
        uiPrint("\r");
    } else {
        int newlines = height - 1 - (rows * 2 + 2) - 1;
        for(int i = 0; i < newlines; i++) {
            uiPrint("\n");
        }
    }

    int spaces = width - 1 - strlen(printMessage);
    for(int i = 0; i < spaces; i++) {
        uiPrint(" ");
    }

    uiPrint("%s", printMessage);
    uiFlush();
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
    if(equalsPos == 0) {
        return;
    }

    *equalsPos = '\0';
    const char* option = line;
    const char* value = equalsPos + 1;
    int val = atoi(value);
    setMenuOption(option, val);
}

const std::string menuPrintConfig() {
    std::stringstream stream;
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(menuList[i].options[j].platforms & MENU_BITMASK && menuList[i].options[j].numValues != 0) {
                stream << menuList[i].options[j].name << "=" << menuList[i].options[j].selection << "\n";
            }
        }
    }

    return stream.str();
}

bool showConsoleDebug() {
    return consoleDebugOutput && !isMenuOn() && !accelPadMode && !(gameboy->isRomLoaded() && gameboy->getRomFile()->isGBS());
}

