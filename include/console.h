#pragma once
#include <stdarg.h>
#include <stdio.h>

extern volatile int consoleSelectedRow; // This line is given a different backdrop

void consoleInitScreens();
void consoleCleanup();

void clearConsole();
int consoleGetWidth();
int consoleGetHeight();

void updateScreens();

void iprintfColored(int palette, const char* format, ...);
void printLog(const char* format, ...);

void disableSleepMode();
void enableSleepMode();

enum {
    CONSOLE_COLOR_BLACK = 0,
    CONSOLE_COLOR_RED,
    CONSOLE_COLOR_GREEN,
    CONSOLE_COLOR_YELLOW,
    CONSOLE_COLOR_BLUE,
    CONSOLE_COLOR_MAGENTA,
    CONSOLE_COLOR_CYAN,
    CONSOLE_COLOR_WHITE,
    CONSOLE_COLOR_BRIGHT_BLACK,
    CONSOLE_COLOR_BRIGHT_RED,
    CONSOLE_COLOR_BRIGHT_GREEN,
    CONSOLE_COLOR_BRIGHT_YELLOW,
    CONSOLE_COLOR_BRIGHT_BLUE,
    CONSOLE_COLOR_BRIGHT_MAGENTA,
    CONSOLE_COLOR_BRIGHT_CYAN,
    CONSOLE_COLOR_BRIGHT_WHITE,
    CONSOLE_COLOR_FAINT_BLACK,
    CONSOLE_COLOR_FAINT_RED,
    CONSOLE_COLOR_FAINT_GREEN,
    CONSOLE_COLOR_FAINT_YELLOW,
    CONSOLE_COLOR_FAINT_BLUE,
    CONSOLE_COLOR_FAINT_MAGENTA,
    CONSOLE_COLOR_FAINT_CYAN,
    CONSOLE_COLOR_FAINT_WHITE
};
