#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include <sstream>

#include "platform/common/lodepng/lodepng.h"
#include "platform/common/config.h"
#include "platform/common/filechooser.h"
#include "platform/common/gbsplayer.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"
#include "gameboy.h"
#include "ppu.h"
#include "romfile.h"

Gameboy* gameboy = NULL;

int fps;
time_t lastPrintTime;

FileChooser romChooser("/", {"gbs", "sgb", "gbc", "cgb", "gb"}, false);
bool chooserInitialized = false;

void mgrInit() {
    gameboy = new Gameboy();
    fps = 0;
    lastPrintTime = 0;
}

void mgrExit() {
    if(gameboy) {
        delete gameboy;
        gameboy = NULL;
    }
}

void mgrLoadRom(const char* filename) {
    if(gameboy == NULL) {
        return;
    }

    if(!gameboy->loadRomFile(filename)) {
        uiClear();
        uiPrint("Error opening %s.", filename);
        uiPrint("\n\nPlease restart GameYob.\n");
        uiFlush();

        while(true) {
            systemCheckRunning();
            uiWaitForVBlank();
        }
    }

    gameboy->loadSave();

    // Border probing is broken
#if 0
    if(sgbBordersEnabled) {
        probingForBorder = true; // This will be ignored if starting in sgb mode, or if there is no sgb mode.
    }
#endif

    gameboy->getPPU()->sgbBorderLoaded = false; // Effectively unloads any sgb border
    mgrRefreshBorder();

    gameboy->init();
    if(gameboy->getRomFile()->isGBS()) {
        gbsPlayerReset();
        gbsPlayerDraw();
    } else if(mgrStateExists(-1)) {
        mgrLoadState(-1);
    }

    if(gameboy->getRomFile()->isGBS()) {
        disableMenuOption("State Slot");
        disableMenuOption("Save State");
        disableMenuOption("Load State");
        disableMenuOption("Delete State");
        disableMenuOption("Suspend");
        disableMenuOption("Exit without saving");
    } else {
        enableMenuOption("State Slot");
        enableMenuOption("Save State");
        enableMenuOption("Suspend");
        if(mgrStateExists(stateNum)) {
            enableMenuOption("Load State");
            enableMenuOption("Delete State");
        } else {
            disableMenuOption("Load State");
            disableMenuOption("Delete State");
        }

        if(gameboy->isRomLoaded() && gameboy->getRomFile()->getRamBanks() > 0 && !gameboy->autosaveEnabled) {
            enableMenuOption("Exit without saving");
        } else {
            disableMenuOption("Exit without saving");
        }

        if(gameboy->isRomLoaded() && gameboy->getRomFile()->getMBC() == MBC7) {
            enableMenuOption("Accelerometer Pad");
        } else {
            disableMenuOption("Accelerometer Pad");
        }
    }
}

void mgrUnloadRom() {
    if(gameboy == NULL || !gameboy->isRomLoaded()) {
        return;
    }

    gameboy->unloadRom();
    gameboy->linkedGameboy = NULL;

    gfxLoadBorder(NULL, 0, 0);
    gfxDrawScreen();
}

void mgrSelectRom() {
    mgrUnloadRom();

    if(!chooserInitialized) {
        chooserInitialized = true;

        DIR* dir = opendir(romPath.c_str());
        if(dir) {
            closedir(dir);
            romChooser.setDirectory(romPath);
        }
    }

    char* filename = romChooser.startFileChooser();
    if(filename == NULL) {
        uiClear();
        uiPrint("Filechooser error");
        uiPrint("\n\nPlease restart GameYob.\n");
        uiFlush();

        while(true) {
            systemCheckRunning();
            uiWaitForVBlank();
        }
    }

    mgrLoadRom(filename);
    free(filename);
}

void mgrLoadBorderFile(const char* filename) {
    // Load the image.
    unsigned char* imgData;
    unsigned int imgWidth;
    unsigned int imgHeight;
    if(lodepng_decode32_file(&imgData, &imgWidth, &imgHeight, filename)) {
        return;
    }

    gfxLoadBorder(imgData, imgWidth, imgHeight);
    free(imgData);
}

void mgrRefreshBorder() {
    // TODO: SGB?

    if(borderSetting == 1) {
        if(gameboy->isRomLoaded()) {
            std::string border = gameboy->getRomFile()->getFileName() + ".png";
            FILE* file = fopen(border.c_str(), "r");
            if(file != NULL) {
                fclose(file);
                mgrLoadBorderFile(border.c_str());
                return;
            }
        }

        FILE* defaultFile = fopen(borderPath.c_str(), "r");
        if(defaultFile != NULL) {
            fclose(defaultFile);
            mgrLoadBorderFile(borderPath.c_str());
            return;
        }
    }

    gfxLoadBorder(NULL, 0, 0);
}

void mgrRefreshBios() {
    gameboy->gbBiosLoaded = false;
    gameboy->gbcBiosLoaded = false;

    FILE* gbFile = fopen(gbBiosPath.c_str(), "rb");
    if(gbFile != NULL) {
        struct stat st;
        fstat(fileno(gbFile), &st);

        if(st.st_size == 0x100) {
            gameboy->gbBiosLoaded = true;
            fread(gameboy->gbBios, 1, 0x100, gbFile);
        }

        fclose(gbFile);
    }

    FILE* gbcFile = fopen(gbcBiosPath.c_str(), "rb");
    if(gbcFile != NULL) {
        struct stat st;
        fstat(fileno(gbcFile), &st);

        if(st.st_size == 0x900) {
            gameboy->gbcBiosLoaded = true;
            fread(gameboy->gbcBios, 1, 0x900, gbcFile);
        }

        fclose(gbFile);
    }
}

const std::string mgrGetStateName(int stateNum) {
    std::stringstream nameStream;
    if(stateNum == -1) {
        nameStream << gameboy->getRomFile()->getFileName() << ".yss";
    } else {
        nameStream << gameboy->getRomFile()->getFileName() << ".ys" << stateNum;
    }

    return nameStream.str();
}

bool mgrStateExists(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return false;
    }

    FILE* file = fopen(mgrGetStateName(stateNum).c_str(), "r");
    if(file != NULL) {
        fclose(file);
    }

    return file != NULL;
}

bool mgrLoadState(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return false;
    }

    FILE* file = fopen(mgrGetStateName(stateNum).c_str(), "r");
    if(file == NULL) {
        return false;
    }

    bool ret = gameboy->loadState(file);
    fclose(file);
    return ret;
}

bool mgrSaveState(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return false;
    }

    FILE* file = fopen(mgrGetStateName(stateNum).c_str(), "w");
    if(file == NULL) {
        return false;
    }

    bool ret = gameboy->saveState(file);
    fclose(file);
    return ret;
}

void mgrDeleteState(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return;
    }

    remove(mgrGetStateName(stateNum).c_str());
}

void mgrSave() {
    if(gameboy == NULL || !gameboy->isRomLoaded()) {
        return;
    }

    gameboy->saveGame();
}

void mgrRun() {
    systemCheckRunning();

    if(gameboy == NULL || !gameboy->isRomLoaded()) {
        return;
    }

    while(!(gameboy->runEmul() & RET_VBLANK));

    gameboy->getPPU()->drawScreen();

    inputUpdate();

    if(isMenuOn()) {
        updateMenu();
    } else {
        if(gameboy->getRomFile()->isGBS()) {
            gbsPlayerRefresh();
        }

        gameboy->gameboyCheckInput();
        if(inputKeyPressed(FUNC_KEY_SAVE)) {
            if(!gameboy->autosaveEnabled) {
                gameboy->saveGame();
            }
        }

        if(inputKeyPressed(FUNC_KEY_FAST_FORWARD_TOGGLE)) {
            gfxToggleFastForward();
        }

        if((inputKeyPressed(FUNC_KEY_MENU) || inputKeyPressed(FUNC_KEY_MENU_PAUSE)) && !accelPadMode) {
            if(pauseOnMenu || inputKeyPressed(FUNC_KEY_MENU_PAUSE)) {
                gameboy->pause();
            }

            gfxSetFastForward(false);
            displayMenu();
        }

        if(inputKeyPressed(FUNC_KEY_SCALE)) {
            setMenuOption("Scaling", (getMenuOption("Scaling") + 1) % 5);
        }

        if(inputKeyPressed(FUNC_KEY_RESET)) {
            gameboy->init();
        }
    }

    time_t rawTime = 0;
    time(&rawTime);
    fps++;
    if(rawTime > lastPrintTime) {
        if(!isMenuOn() && !showConsoleDebug() && (!gameboy->isRomLoaded() || !gameboy->getRomFile()->isGBS()) && (fpsOutput || timeOutput)) {
            uiClear();
            int fpsLength = 0;
            if(fpsOutput) {
                char buffer[16];
                snprintf(buffer, 16, "FPS: %d", fps);
                uiPrint("%s", buffer);
                fpsLength = (int) strlen(buffer);
            }

            if(timeOutput) {
                char *timeString = ctime(&rawTime);
                for(int i = 0; ; i++) {
                    if(timeString[i] == ':') {
                        timeString += i - 2;
                        break;
                    }
                }

                char timeDisplay[6] = {0};
                strncpy(timeDisplay, timeString, 5);

                int spaces = uiGetWidth() - (int) strlen(timeDisplay) - fpsLength;
                for(int i = 0; i < spaces; i++) {
                    uiPrint(" ");
                }

                uiPrint("%s", timeDisplay);
            }

            uiPrint("\n");

            uiFlush();
        }

        fps = 0;
        lastPrintTime = rawTime;
    }
}
