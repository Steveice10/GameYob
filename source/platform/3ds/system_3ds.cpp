#ifdef BACKEND_3DS

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/audio.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"

#include <ctrcommon/fs.hpp>
#include <ctrcommon/ir.hpp>
#include <ctrcommon/platform.hpp>
#include <ctrcommon/socket.hpp>

bool systemInit(int argc, char* argv[]) {
    if(!platformInit(argc) || !gfxInit()) {
        return 0;
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

const std::string systemIniPath() {
    return "/gameyob.ini";
}

const std::string systemDefaultGbBiosPath() {
    return "/gb_bios.bin";
}

const std::string systemDefaultGbcBiosPath() {
    return "/gbc_bios.bin";
}

const std::string systemDefaultBorderPath() {
    return "/default_border.png";
}

const std::string systemDefaultRomPath() {
    return "/gb/";
}

void systemPrintDebug(const char* str, ...) {
    if(showConsoleDebug()) {
        va_list list;
        va_start(list, str);
        uiPrintv(str, list);
        va_end(list);
    }
}

bool systemGetIRState() {
    return irGetState() == 1;
}

void systemSetIRState(bool state) {
    irSetState(state ? 1 : 0);
}

const std::string systemGetIP() {
    return inet_ntoa({socketGetHostIP()});
}

int systemSocketListen(u16 port) {
    return socketListen(port);
}

FILE* systemSocketAccept(int listeningSocket, std::string* acceptedIp) {
    return socketAccept(listeningSocket, acceptedIp);
}

FILE* systemSocketConnect(const std::string ipAddress, u16 port) {
    return socketConnect(ipAddress, port);
}

#endif