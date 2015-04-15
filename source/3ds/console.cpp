#include <stdio.h>
#include <3ds.h>
#include "console.h"
#include "menu.h"
#include "gbgfx.h"
#include "inputhelper.h"

volatile int consoleSelectedRow;


bool isConsoleOn() {
    return true;
}

void clearConsole() {
	iprintf("\x1b[2J");
}

GYPrintConsole* getDefaultConsole() {
    return NULL;
}

void updateScreens(bool waitToFinish) {
    consoleSetScreen((gfxScreen_t)!gameScreen);
    checkBorder();
}


void consoleSetPosColor(int x, int y, int color) {

}

void consoleSetLineColor(int line, int color) {

}

void iprintfColored(int palette, const char* format, ...) {
    va_list args;
    va_start(args, format);

    vprintf(format, args);
    va_end(args);
}
void printLog(const char* format, ...) {
    if (consoleDebugOutput) {
        va_list args;
        va_start(args, format);

        vprintf(format, args);
        va_end(args);
    }
}
void printAndWait(const char* format, ...) {
    va_list args;
    va_start(args, format);

    vprintf(format, args);
    va_end(args);

    inputUpdateVBlank();
    while (!keyJustPressed(KEY_A)) {
        system_checkPolls();
        inputUpdateVBlank();
    }
}

int checkRumble() {
    return 0;
}


void disableSleepMode() {

}
void enableSleepMode() {

}

void setPrintConsole(GYPrintConsole* console) {

}
GYPrintConsole* getPrintConsole() {
    return NULL;
}
