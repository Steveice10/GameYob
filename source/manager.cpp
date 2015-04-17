#include <stdlib.h>

#include "platform/input.h"
#include "platform/system.h"
#include "filechooser.h"
#include "gameboy.h"
#include "manager.h"
#include "menu.h"

Gameboy* gameboy = NULL;

int fps = 0;

time_t rawTime;
time_t lastRawTime;

void mgrInit() {
    gameboy = new Gameboy();

    rawTime = 0;
    lastRawTime = rawTime;
}

void mgrRunFrame() {
    int ret1 = 0;

    bool paused = false;
    while(!paused && !(ret1 & RET_VBLANK)) {
        paused = false;
        if(gameboy && gameboy->isGameboyPaused())
            paused = true;
        if(!paused) {
            if(gameboy) {
                if(!(ret1 & RET_VBLANK))
                    ret1 |= gameboy->runEmul();
            }
            else
                ret1 |= RET_VBLANK;
        }
    }
}

void mgrLoadRom(const char* filename) {
    gameboy->setRomFile(filename);
    gameboy->loadSave(1);

    // Border probing is broken
#if 0
    if (sgbBordersEnabled)
        probingForBorder = true; // This will be ignored if starting in sgb mode, or if there is no sgb mode.
#endif

    gameboy->getPPU()->sgbBorderLoaded = false; // Effectively unloads any sgb border

    gameboy->init();

    if(gameboy->getGBSPlayer()->gbsMode) {
        disableMenuOption("State Slot");
        disableMenuOption("Save State");
        disableMenuOption("Load State");
        disableMenuOption("Delete State");
        disableMenuOption("Suspend");
        disableMenuOption("Exit without saving");
    }
    else {
        enableMenuOption("State Slot");
        enableMenuOption("Save State");
        enableMenuOption("Suspend");
        if(gameboy->checkStateExists(stateNum)) {
            enableMenuOption("Load State");
            enableMenuOption("Delete State");
        }
        else {
            disableMenuOption("Load State");
            disableMenuOption("Delete State");
        }

        if(gameboy->getNumRamBanks() && !autoSavingEnabled)
            enableMenuOption("Exit without saving");
        else
            disableMenuOption("Exit without saving");
    }

    if(gameboy->getRomFile()->getMBC() == MBC7) {
        enableMenuOption("Accelerometer Pad");
    } else {
        disableMenuOption("Accelerometer Pad");
    }
}

void mgr_unloadRom() {
    gameboy->unloadRom();
    gameboy->linkedGameboy = NULL;
}

void mgrSelectRom() {
    mgr_unloadRom();

    loadFileChooserState(&romChooserState);
    const char* extraExtensions[] = {"gbs"};
    char* filename = startFileChooser(extraExtensions, true);
    saveFileChooserState(&romChooserState);

    if(filename == NULL) {
        printf("Filechooser error");
        printf("\n\nPlease restart GameYob.\n");
        while(true) {
            systemCheckRunning();
        }
    }

    mgrLoadRom(filename);

    free(filename);

    systemUpdateConsole();
}

void mgrSave() {
    if(gameboy)
        gameboy->saveGame();
}

void mgrUpdateVBlank() {
    gameboy->getPPU()->drawScreen();

    systemCheckRunning();

    if(gameboy && !gameboy->isGameboyPaused())
        gameboy->getAPU()->soundUpdateVBlank();

    inputUpdate();

    if(isMenuOn())
        updateMenu();
    else {
        gameboy->gameboyCheckInput();
        if(gameboy->getGBSPlayer()->gbsMode)
            gameboy->getGBSPlayer()->gbsCheckInput();
    }

    time(&rawTime);
    fps++;
    if(!isMenuOn() && !consoleDebugOutput && rawTime > lastRawTime) {
        if(fpsOutput) {
            iprintf("\x1b[2J");
            iprintf("FPS: %d\n", fps);
        }

        fps = 0;
        lastRawTime = rawTime;
    }
}

void mgrExit() {
    if(gameboy)
        delete gameboy;

    gameboy = NULL;
}
