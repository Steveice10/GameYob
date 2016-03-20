#ifdef BACKEND_3DS

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>

#include <citrus/core.hpp>

#include <3ds.h>

#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/audio.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"

using namespace ctr;

static bool requestedExit;

static u32* iruBuffer;

bool systemInit(int argc, char* argv[]) {
    requestedExit = false;

    if(!core::init(argc) || !gfxInit()) {
        return 0;
    }

    audioInit();
    uiInit();
    inputInit();

    iruBuffer = (u32*) memalign(0x1000, 0x1000);
    if(iruBuffer != NULL) {
        if(R_FAILED(iruInit(iruBuffer, 0x1000))) {
            free(iruBuffer);
        }
    }


    return true;
}

void systemExit() {
    if(iruBuffer != NULL) {
        iruExit();
        free(iruBuffer);
        iruBuffer = NULL;
    }

    inputCleanup();
    uiCleanup();
    audioCleanup();
    gfxCleanup();

    core::exit();
}

void systemRun() {
    mgrRun();
}

bool systemIsRunning() {
    return !requestedExit && core::running();
}

void systemRequestExit() {
    requestedExit = true;
}

const std::string systemIniPath() {
    return "gameyob.ini";
}

const std::string systemDefaultGbBiosPath() {
    return "gb_bios.bin";
}

const std::string systemDefaultGbcBiosPath() {
    return "gbc_bios.bin";
}

const std::string systemDefaultBorderPath() {
    return "default_border.png";
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

        uiFlush();
    }
}

bool systemGetIRState() {
    if(iruBuffer == NULL) {
        return false;
    }

    u32 state = 0;
    Result res = IRU_GetIRLEDRecvState(&state);
    if(R_FAILED(res)) {
        systemPrintDebug("IR receive failed: 0x%08lx\n", res);
    }

    return state == 1;
}

void systemSetIRState(bool state) {
    if(iruBuffer == NULL) {
        return;
    }

    Result res = IRU_SetIRLEDState(state ? 1 : 0);
    if(R_FAILED(res)) {
        systemPrintDebug("IR send failed: 0x%08lx\n", res);
    }
}

const std::string systemGetIP() {
    return inet_ntoa({(u32) gethostid()});
}

int systemSocketListen(u16 port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        return -1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if(bind(fd, (struct sockaddr*) &address, sizeof(address)) != 0) {
        closesocket(fd);
        return -1;
    }

    int flags = fcntl(fd, F_GETFL);
    if(flags == -1) {
        closesocket(fd);
        return -1;
    }

    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        closesocket(fd);
        return -1;
    }

    if(listen(fd, 10) != 0) {
        closesocket(fd);
        return -1;
    }

    return fd;
}

FILE* systemSocketAccept(int listeningSocket, std::string* acceptedIp) {
    struct sockaddr_in addr;
    socklen_t addrSize = sizeof(addr);
    int afd = accept(listeningSocket, (struct sockaddr*) &addr, &addrSize);
    if(afd < 0) {
        return NULL;
    }

    if(acceptedIp != NULL) {
        *acceptedIp = inet_ntoa(addr.sin_addr);
    }

    int flags = fcntl(afd, F_GETFL);
    if(flags == -1) {
        closesocket(afd);
        return NULL;
    }

    if(fcntl(afd, F_SETFL, flags | O_NONBLOCK) != 0) {
        closesocket(afd);
        return NULL;
    }

    return fdopen(afd, "rw");
}

FILE* systemSocketConnect(const std::string ipAddress, u16 port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        return NULL;
    }

    int flags = fcntl(fd, F_GETFL);
    if(flags == -1) {
        closesocket(fd);
        return NULL;
    }

    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        closesocket(fd);
        return NULL;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if(inet_aton(ipAddress.c_str(), &address.sin_addr) <= 0) {
        closesocket(fd);
        return NULL;
    }

    if(connect(fd, (struct sockaddr*) &address, sizeof(address)) < 0) {
        if(errno != EINPROGRESS) {
            closesocket(fd);
            return NULL;
        }
    }

    struct pollfd pollinfo;
    pollinfo.fd = fd;
    pollinfo.events = POLLOUT;
    pollinfo.revents = 0;
    int pollRet = poll(&pollinfo, 1, 10000);
    if(pollRet <= 0) {
        if(pollRet == 0) {
            errno = ETIMEDOUT;
        }

        closesocket(fd);
        return NULL;
    }

    return fdopen(fd, "rw");
}

void systemSetRumble(bool rumble) {
}

u32* systemGetCameraImage() {
    return NULL;
}

#endif