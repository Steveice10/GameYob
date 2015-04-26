#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "platform/audio.h"
#include "platform/gfx.h"
#include "platform/system.h"
#include "ui/config.h"
#include "ui/manager.h"
#include "ui/menu.h"

#include <3ds.h>
#include <ctrcommon/fs.hpp>
#include <ctrcommon/ir.hpp>
#include <ctrcommon/platform.hpp>
#include <ctrcommon/screen.hpp>

gfxScreen_t currConsole;

PrintConsole* topConsole;
PrintConsole* bottomConsole;

bool systemInit() {
    if(!platformInit() || !gfxInit()) {
        return 0;
    }

    topConsole = (PrintConsole*) calloc(1, sizeof(PrintConsole));
    consoleInit(GFX_TOP, topConsole);

    bottomConsole = (PrintConsole*) calloc(1, sizeof(PrintConsole));
    consoleInit(GFX_BOTTOM, bottomConsole);

    gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
    gfxSetDoubleBuffering(GFX_TOP, true);

    consoleSelect(bottomConsole);
    currConsole = GFX_BOTTOM;

    mgrInit();

    setMenuDefaults();
    readConfigFile();

    printf("GameYob 3DS\n\n");

    return true;
}

void systemExit() {
    mgrSave();
    mgrExit();

    free(topConsole);
    free(bottomConsole);

    gfxCleanup();
    platformCleanup();

    exit(0);
}

void systemRun() {
    mgrSelectRom();
    while(true) {
        mgrRun();
    }
}

void systemCheckRunning() {
    if(!platformIsRunning()) {
        systemExit();
    }
}

int systemGetConsoleWidth() {
    PrintConsole* console = !gameScreen == 0 ? topConsole : bottomConsole;
    return console->consoleWidth;
}

int systemGetConsoleHeight() {
    PrintConsole* console = !gameScreen == 0 ? topConsole : bottomConsole;
    return console->consoleHeight;
}

void systemUpdateConsole() {
    gfxScreen_t screen = !gameScreen == 0 ? GFX_TOP : GFX_BOTTOM;
    if(currConsole != screen) {
        gfxScreen_t oldScreen = gameScreen == 0 ? GFX_TOP : GFX_BOTTOM;
        gfxSetScreenFormat(oldScreen, GSP_BGR8_OES);
        gfxSetDoubleBuffering(oldScreen, true);

        screenClearAll(0, 0, 0);

        currConsole = screen;
        gfxSetScreenFormat(screen, GSP_RGB565_OES);
        gfxSetDoubleBuffering(screen, false);

        gfxSwapBuffers();
        gspWaitForVBlank();

        u16* framebuffer = (u16*) gfxGetFramebuffer(screen, GFX_LEFT, NULL, NULL);
        PrintConsole* console = screen == GFX_TOP ? topConsole : bottomConsole;
        console->frameBuffer = framebuffer;
        consoleSelect(console);
    }
}

u32 systemGetIRState() {
    return !irGetState();
}

void systemSetIRState(u32 state) {
    irSetState(state);
}