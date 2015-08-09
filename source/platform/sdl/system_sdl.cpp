#ifdef BACKEND_SDL

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/gfx.h"
#include "platform/system.h"

#include <SDL2/SDL.h>
#include <platform/ui.h>

extern void gfxSetWindowSize(int width, int height);

bool systemInit(int argc, char* argv[]) {
    if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0 || !gfxInit()) {
        return false;
    }

    uiInit();
    inputInit();
    audioInit();

    mgrInit();

    setMenuDefaults();
    readConfigFile();

    return true;
}

void systemExit() {
    mgrSave();
    mgrExit();

    audioCleanup();
    inputCleanup();
    uiCleanup();
    gfxCleanup();

    SDL_Quit();

    exit(0);
}

void systemRun() {
    mgrSelectRom();
    while(true) {
        mgrRun();
    }
}

void systemCheckRunning() {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_QUIT) {
            systemExit();
            return;
        } else if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            gfxSetWindowSize(event.window.data1, event.window.data2);
        }
    }
}

const std::string systemIniPath() {
    return "./gameyob.ini";
}

const std::string systemDefaultGbBiosPath() {
    return "./gb_bios.bin";
}

const std::string systemDefaultGbcBiosPath() {
    return "./gbc_bios.bin";
}

const std::string systemDefaultBorderPath() {
    return "./default_border.png";
}

const std::string systemDefaultRomPath() {
    return "./gb/";
}

void systemPrintDebug(const char* str, ...) {
    if(showConsoleDebug()) {
        va_list list;
        va_start(list, str);
        uiPrintv(str, list);
        va_end(list);

        uiFlush();
    }
}

bool systemGetIRState() {
    return false;
}

void systemSetIRState(bool state) {
}

const std::string systemGetIP() {
    return "";
}

int systemSocketListen(u16 port) {
    return -1;
}

FILE* systemSocketAccept(int listeningSocket, std::string* acceptedIp) {
    return NULL;
}

FILE* systemSocketConnect(const std::string ipAddress, u16 port) {
    return NULL;
}

#endif