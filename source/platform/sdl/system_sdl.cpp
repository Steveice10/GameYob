#ifdef BACKEND_SDL

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/gfx.h"
#include "platform/system.h"
#include "platform/ui.h"

#include <SDL2/SDL.h>

extern void gfxSetWindowSize(int width, int height);

static bool requestedExit;

bool systemInit(int argc, char* argv[]) {
    if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0 || !gfxInit()) {
        return false;
    }

    audioInit();
    uiInit();
    inputInit();

    requestedExit = false;

    return true;
}

void systemExit() {
    inputCleanup();
    uiCleanup();
    audioCleanup();
    gfxCleanup();

    SDL_Quit();
}

void systemRun() {
    mgrRun();
}

bool systemIsRunning() {
    if(requestedExit) {
        return false;
    }

    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_QUIT) {
            requestedExit = true;
            return false;
        } else if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            gfxSetWindowSize(event.window.data1, event.window.data2);
        }
    }

    return true;
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
    return "gb/";
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
        close(fd);
        return -1;
    }

    int flags = fcntl(fd, F_GETFL);
    if(flags == -1) {
        close(fd);
        return -1;
    }

    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(fd);
        return -1;
    }

    if(listen(fd, 10) != 0) {
        close(fd);
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
        close(afd);
        return NULL;
    }

    if(fcntl(afd, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(afd);
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
        close(fd);
        return NULL;
    }

    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(fd);
        return NULL;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if(inet_aton(ipAddress.c_str(), &address.sin_addr) <= 0) {
        close(fd);
        return NULL;
    }

    if(connect(fd, (struct sockaddr*) &address, sizeof(address)) < 0) {
        if(errno != EINPROGRESS) {
            close(fd);
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

        close(fd);
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