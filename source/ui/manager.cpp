#include <stdlib.h>
#include <string.h>
#include <sys/dirent.h>

#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"
#include "ui/config.h"
#include "ui/filechooser.h"
#include "ui/gbsplayer.h"
#include "ui/manager.h"
#include "ui/menu.h"

Gameboy* gameboy = NULL;

int fps;
time_t lastPrintTime;

FileChooser romChooser("/");
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

    gameboy->setRomFile(filename);
    gameboy->loadSave(1);

    // Border probing is broken
#if 0
    if(sgbBordersEnabled) {
        probingForBorder = true; // This will be ignored if starting in sgb mode, or if there is no sgb mode.
    }
#endif

    gameboy->getPPU()->sgbBorderLoaded = false; // Effectively unloads any sgb border

    gameboy->init();

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
        if(gameboy->checkStateExists(stateNum)) {
            enableMenuOption("Load State");
            enableMenuOption("Delete State");
        } else {
            disableMenuOption("Load State");
            disableMenuOption("Delete State");
        }

        if(gameboy->isRomLoaded() && gameboy->getRomFile()->getRamBanks() > 0 && !autoSavingEnabled) {
            enableMenuOption("Exit without saving");
        } else {
            disableMenuOption("Exit without saving");
        }

        if(gameboy->biosLoaded) {
            enableMenuOption("GBC Bios");
        } else {
            disableMenuOption("GBC Bios");
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

    gfxLoadBorder(NULL);
    memset(gfxGetScreenBuffer(), 0x00, 256 * 256 * sizeof(u32));
    gfxDrawScreen();
}

void mgrSelectRom() {
    mgrUnloadRom();

    if(!chooserInitialized) {
        chooserInitialized = true;

        DIR* dir = opendir(romPath);
        if(dir) {
            closedir(dir);
            romChooser.setDirectory(romPath);
        }
    }

    const char* extraExtensions[] = {"gbs"};
    char* filename = romChooser.startFileChooser(extraExtensions, true);

    if(filename == NULL) {
        printf("Filechooser error");
        printf("\n\nPlease restart GameYob.\n");
        while(true) {
            systemCheckRunning();
            gfxWaitForVBlank();
        }
    }

    mgrLoadRom(filename);
    free(filename);
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

    while(!gameboy->isGameboyPaused() && !(gameboy->runEmul() & RET_VBLANK));

    gameboy->getPPU()->drawScreen();

    inputUpdate();

    if(isMenuOn()) {
        updateMenu();
    } else {
        if(gameboy->getRomFile()->isGBS()) {
            gbsPlayerRefresh();
        }

        gameboy->gameboyCheckInput();
    }

    time_t rawTime = 0;
    time(&rawTime);
    fps++;
    if(rawTime > lastPrintTime) {
        if(!isMenuOn() && !showConsoleDebug() && (!gameboy->isRomLoaded() || !gameboy->getRomFile()->isGBS()) && (fpsOutput || timeOutput)) {
            iprintf("\x1b[2J");
            int fpsLength = 0;
            if(fpsOutput) {
                char buffer[16];
                snprintf(buffer, 16, "FPS: %d", fps);
                printf(buffer);
                fpsLength = strlen(buffer);
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

                int spaces = systemGetConsoleWidth() - strlen(timeDisplay) - fpsLength;
                for(int i = 0; i < spaces; i++) {
                    printf(" ");
                }

                printf("%s", timeDisplay);
            }

            printf("\n");
        }

        fps = 0;
        lastPrintTime = rawTime;
    }
}
