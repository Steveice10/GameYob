#pragma once
#include <stdarg.h>
#include <stdio.h>

#ifdef _3DS
#include "3ds/printconsole.h"
#endif

extern GYPrintConsole* menuConsole;

extern volatile int consoleSelectedRow; // This line is given a different backdrop


bool isConsoleOn(); // Returns true if the sub-screen's console is set up.

void clearConsole();
void consoleFlush();
GYPrintConsole* getDefaultConsole();
int consoleGetWidth();
int consoleGetHeight();

void updateScreens(bool waitToFinish=false);


void consoleSetPosColor(int x, int y, int color);
void consoleSetLineColor(int line, int color);
void iprintfColored(int palette, const char* format, ...);
void printLog(const char* format, ...);

void printAndWait(const char* format, ...); // Used for debugging

int checkRumble();


void disableSleepMode();
void enableSleepMode();

void setPrintConsole(GYPrintConsole* console);
GYPrintConsole* getPrintConsole();


enum {
    CONSOLE_COLOR_BLACK=0,
    CONSOLE_COLOR_RED,
    CONSOLE_COLOR_GREEN,
    CONSOLE_COLOR_YELLOW,
    CONSOLE_COLOR_BLUE,
    CONSOLE_COLOR_MAGENTA,
    CONSOLE_COLOR_CYAN,
    CONSOLE_COLOR_GREY,
    CONSOLE_COLOR_LIGHT_BLACK,
    CONSOLE_COLOR_LIGHT_RED,
    CONSOLE_COLOR_LIGHT_GREEN,
    CONSOLE_COLOR_LIGHT_YELLOW,
    CONSOLE_COLOR_LIGHT_BLUE,
    CONSOLE_COLOR_LIGHT_MAGENTA,
    CONSOLE_COLOR_LIGHT_CYAN,
    CONSOLE_COLOR_WHITE
};
