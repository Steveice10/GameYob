#ifdef BACKEND_3DS

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <queue>

#include "platform/common/menu.h"
#include "platform/ui.h"

#include <citrus/gpu.hpp>

#include <3ds.h>

using namespace ctr;

gfxScreen_t currConsole;

PrintConsole* topConsole;
PrintConsole* bottomConsole;

TextColor textColor;
bool highlighted;

std::queue<UIKey> keyQueue;

void uiInit() {
    topConsole = (PrintConsole*) calloc(1, sizeof(PrintConsole));
    consoleInit(GFX_TOP, topConsole);

    bottomConsole = (PrintConsole*) calloc(1, sizeof(PrintConsole));
    consoleInit(GFX_BOTTOM, bottomConsole);

    consoleDebugInit(debugDevice_3DMOO);

    gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
    gfxSetDoubleBuffering(GFX_TOP, true);

    consoleSelect(bottomConsole);
    currConsole = GFX_BOTTOM;
}

void uiCleanup() {
    free(topConsole);
    free(bottomConsole);
}

void uiUpdateScreen() {
    gfxScreen_t screen = !gameScreen == 0 ? GFX_TOP : GFX_BOTTOM;
    if(currConsole != screen) {
        gfxScreen_t oldScreen = gameScreen == 0 ? GFX_TOP : GFX_BOTTOM;
        gfxSetScreenFormat(oldScreen, GSP_BGR8_OES);
        gfxSetDoubleBuffering(oldScreen, true);

        for(u32 i = 0; i < 2; i++) {
            gpu::setViewport(gpu::SCREEN_TOP, 0, 0, gpu::TOP_WIDTH, gpu::TOP_HEIGHT);
            gpu::clear();
            gpu::flushBuffer();

            gpu::setViewport(gpu::SCREEN_BOTTOM, 0, 0, gpu::BOTTOM_WIDTH, gpu::BOTTOM_HEIGHT);
            gpu::clear();
            gpu::flushBuffer();

            gpu::swapBuffers(false);
        }

        gpu::setViewport(gpu::SCREEN_TOP, 0, 0, gpu::TOP_WIDTH, gpu::TOP_HEIGHT);

        currConsole = screen;
        gfxSetScreenFormat(screen, GSP_RGB565_OES);
        gfxSetDoubleBuffering(screen, false);

        gfxSwapBuffers();
        gspWaitForVBlank();

        PrintConsole* console = screen == GFX_TOP ? topConsole : bottomConsole;
        console->frameBuffer = (u16*) gfxGetFramebuffer(screen, GFX_LEFT, NULL, NULL);
        consoleSelect(console);
    }
}

int uiGetWidth() {
    PrintConsole* console = !gameScreen == 0 ? topConsole : bottomConsole;
    return console->consoleWidth;
}

int uiGetHeight() {
    PrintConsole* console = !gameScreen == 0 ? topConsole : bottomConsole;
    return console->consoleHeight;
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
}

void uiWaitForVBlank() {
    gspWaitForVBlank();
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
