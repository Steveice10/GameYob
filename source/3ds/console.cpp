#include <stdio.h>
#include <3ds.h>
#include <3ds/3dsgfx.h>
#include <malloc.h>
#include "console.h"
#include "menu.h"
#include "gbgfx.h"
#include "inputhelper.h"

gfxScreen_t currConsole;

PrintConsole* topConsole;
PrintConsole* bottomConsole;

void consoleInitScreens() {
    topConsole = (PrintConsole*) calloc(1, sizeof(PrintConsole));
    consoleInit(GFX_TOP, topConsole);

    bottomConsole = (PrintConsole*) calloc(1, sizeof(PrintConsole));
    consoleInit(GFX_BOTTOM, bottomConsole);

    gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
    gfxSetDoubleBuffering(GFX_TOP, true);

    consoleSelect(bottomConsole);
    currConsole = GFX_BOTTOM;
}

int consoleGetWidth() {
    PrintConsole* console = !gameScreen == 0 ? topConsole : bottomConsole;
    return console->consoleWidth;
}

int consoleGetHeight() {
    PrintConsole* console = !gameScreen == 0 ? topConsole : bottomConsole;
    return console->consoleHeight;
}

void clearConsole() {
	consoleClear();
}

void updateScreens() {
    gfxScreen_t screen = !gameScreen == 0 ? GFX_TOP : GFX_BOTTOM;
    if(currConsole != screen) {
        currConsole = screen;
        gfxSetScreenFormat(screen, GSP_RGB565_OES);
        gfxSetDoubleBuffering(screen, false);

        gfxScreen_t oldScreen = gameScreen == 0 ? GFX_TOP : GFX_BOTTOM;
        gfxSetScreenFormat(oldScreen, GSP_BGR8_OES);
        gfxSetDoubleBuffering(oldScreen, true);

        gfxSwapBuffers();
        gspWaitForVBlank();

        u16 *framebuffer = (u16 *) gfxGetFramebuffer(screen, GFX_LEFT, NULL, NULL);
        PrintConsole *console = screen == GFX_TOP ? topConsole : bottomConsole;
        console->frameBuffer = framebuffer;
        consoleSelect(console);

        checkBorder();
    }
}

void iprintfColored(int palette, const char* format, ...) {
    PrintConsole* console = !gameScreen == 0 ? topConsole : bottomConsole;
    console->fg = palette;

    va_list args;
    va_start(args, format);

    vprintf(format, args);
    va_end(args);

    console->fg = CONSOLE_COLOR_WHITE;
}

void printLog(const char* format, ...) {
    if(consoleDebugOutput) {
        va_list args;
        va_start(args, format);

        vprintf(format, args);
        va_end(args);
    }
}

void disableSleepMode() {
}

void enableSleepMode() {
}