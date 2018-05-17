#ifdef BACKEND_SWITCH

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <queue>

#include <switch.h>

#include "platform/common/menu.h"
#include "platform/ui.h"

static TextColor textColor;
static bool highlighted;

static std::queue<UIKey> keyQueue;

void uiInit() {
    consoleInit(NULL);
    consoleDebugInit(debugDevice_SVC);
}

void uiCleanup() {
}

void uiUpdateScreen() {
}

void uiGetSize(int* width, int* height) {
    PrintConsole* console = consoleGetDefault();

    if(width != NULL) {
        *width = console->consoleWidth;
    }

    if(height != NULL) {
        *height = console->consoleHeight;
    }
}

void uiClear() {
    consoleClear();
}

void uiUpdateAttr() {
    if(highlighted) {
        printf("\x1b[0m\x1b[47m");
        switch(textColor) {
            case TEXT_COLOR_NONE:
                printf("\x1b[30m");
                break;
            case TEXT_COLOR_YELLOW:
                printf("\x1b[33m");
                break;
            case TEXT_COLOR_RED:
                printf("\x1b[31m");
                break;
            case TEXT_COLOR_GREEN:
                printf("\x1b[32m");
                break;
            case TEXT_COLOR_PURPLE:
                printf("\x1b[35m");
                break;
            case TEXT_COLOR_GRAY:
                printf("\x1b[2m\x1b[37m");
                break;
            case TEXT_COLOR_WHITE:
                printf("\x1b[30m");
                break;
        }
    } else {
        printf("\x1b[0m\x1b[40m");
        switch(textColor) {
            case TEXT_COLOR_NONE:
                printf("\x1b[37m");
                break;
            case TEXT_COLOR_YELLOW:
                printf("\x1b[1m\x1b[33m");
                break;
            case TEXT_COLOR_RED:
                printf("\x1b[1m\x1b[31m");
                break;
            case TEXT_COLOR_GREEN:
                printf("\x1b[1m\x1b[32m");
                break;
            case TEXT_COLOR_PURPLE:
                printf("\x1b[1m\x1b[35m");
                break;
            case TEXT_COLOR_GRAY:
                printf("\x1b[2m\x1b[37m");
                break;
            case TEXT_COLOR_WHITE:
                printf("\x1b[37m");
                break;
        }
    }
}

void uiSetLineHighlighted(bool highlight) {
    highlighted = highlight;
    uiUpdateAttr();
}

void uiSetTextColor(TextColor color) {
    textColor = color;
    uiUpdateAttr();
}

void uiPrint(const char* str, ...) {
    va_list list;
    va_start(list, str);
    uiPrintv(str, list);
    va_end(list);
}

void uiPrintv(const char* str, va_list list) {
    vprintf(str, list);
}

void uiFlush() {
    gfxFlushBuffers();
    gfxSwapBuffers();
}

void uiSync() {
    gfxWaitForVsync();
}

void uiPushInput(UIKey key) {
    keyQueue.push(key);
}

UIKey uiReadKey() {
    if(!keyQueue.empty()) {
        UIKey key = keyQueue.front();
        keyQueue.pop();
        return key;
    }

    return UI_KEY_NONE;
}

#endif
