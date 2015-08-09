#pragma once

#include "types.h"

typedef enum {
    TEXT_COLOR_NONE = 0,
    TEXT_COLOR_YELLOW = 1,
    TEXT_COLOR_RED = 2,
    TEXT_COLOR_GREEN = 3,
    TEXT_COLOR_PURPLE = 4,
    TEXT_COLOR_GRAY = 5,
    TEXT_COLOR_WHITE = 6
} TextColor;

typedef enum {
    UI_KEY_NONE = 0,
    UI_KEY_A = 1,
    UI_KEY_B = 2,
    UI_KEY_UP = 3,
    UI_KEY_DOWN = 4,
    UI_KEY_LEFT = 5,
    UI_KEY_RIGHT = 6,
    UI_KEY_L = 7,
    UI_KEY_R = 8,
    UI_KEY_X = 9,
    UI_KEY_Y = 10
} UIKey;

void uiInit();
void uiCleanup();
void uiUpdateScreen();
int uiGetWidth();
int uiGetHeight();
void uiClear();
void uiSetLineHighlighted(bool highlight);
void uiSetTextColor(TextColor color);
void uiPrint(const char* str, ...);
void uiPrintv(const char* str, va_list list);
void uiFlush();
void uiWaitForVBlank();
UIKey uiReadKey();