#ifdef BACKEND_3DS

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include <fstream>

#include <3ds.h>

#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/audio.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"
#include "printer.h"

static bool requestedExit;

static u32* iruBuffer;
static u32* socBuffer;

static int numPrinted;

bool systemInit(int argc, char* argv[]) {
    if(!gfxInit()) {
        return false;
    }

    audioInit();
    uiInit();
    inputInit();

    iruBuffer = (u32*) memalign(0x1000, 0x1000);
    if(iruBuffer != NULL) {
        if(R_FAILED(iruInit(iruBuffer, 0x1000))) {
            free(iruBuffer);
            iruBuffer = NULL;
        }
    }

    socBuffer = (u32*) memalign(0x1000, 0x100000);
    if(socBuffer != NULL) {
        if(R_FAILED(socInit(socBuffer, 0x100000))) {
            free(socBuffer);
            socBuffer = NULL;
        }
    }

    numPrinted = 0;

    requestedExit = false;

    return true;
}

void systemExit() {
    if(socBuffer != NULL) {
        socExit();
        free(socBuffer);
        socBuffer = NULL;
    }

    if(iruBuffer != NULL) {
        iruExit();
        free(iruBuffer);
        iruBuffer = NULL;
    }

    inputCleanup();
    uiCleanup();
    audioCleanup();
    gfxCleanup();
}

bool systemIsRunning() {
    return !requestedExit && aptMainLoop();
}

void systemRequestExit() {
    requestedExit = true;
}

const std::string systemIniPath() {
    return "gameyob.ini";
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
    if(socBuffer == NULL) {
        return -1;
    }

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
    if(socBuffer == NULL) {
        return NULL;
    }

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
    if(socBuffer == NULL) {
        return NULL;
    }

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

void systemPrintImage(bool appending, u8* buf, int size, u8 palette) {
    // Find the first available "print number".
    char filename[300];
    while(true) {
        snprintf(filename, 300, "%s-%d.bmp", mgrGetRomName().c_str(), numPrinted);

        // If appending, the last file written to is already selected.
        // Else, if the file doesn't exist, we're done searching.
        if(appending || access(filename, R_OK) != 0) {
            if(appending && access(filename, R_OK) != 0) {
                // This is a failsafe, this shouldn't happen
                appending = false;
                systemPrintDebug("The image to be appended to doesn't exist: %s\n", filename);
                continue;
            } else {
                break;
            }
        }

        numPrinted++;
    }

    int width = PRINTER_WIDTH;

    // In case of error, size must be rounded off to the nearest 16 vertical pixels.
    if(size % (width / 4 * 16) != 0) {
        size += (width / 4 * 16) - (size % (width / 4 * 16));
    }

    int height = size / width * 4;
    int pixelArraySize = (width * height + 1) / 2;

    u8 bmpHeader[] = { // Contains header data & palettes
            0x42, 0x4d, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x28, 0x00,
            0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x04, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa,
            0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff
    };

    // Set up the palette
    for(int i = 0; i < 4; i++) {
        u8 rgb = 0;
        switch((palette >> (i * 2)) & 3) {
            case 0:
                rgb = 0xff;
                break;
            case 1:
                rgb = 0xaa;
                break;
            case 2:
                rgb = 0x55;
                break;
            case 3:
                rgb = 0x00;
                break;
            default:
                break;
        }

        for(int j = 0; j < 4; j++) {
            bmpHeader[0x36 + i * 4 + j] = rgb;
        }
    }

    u16* pixelData = (u16*) malloc((size_t) pixelArraySize);

    // Convert the gameboy's tile-based 2bpp into a linear 4bpp format.
    for(int i = 0; i < size; i += 2) {
        u8 b1 = buf[i];
        u8 b2 = buf[i + 1];

        int pixel = i * 4;
        int tile = pixel / 64;

        int index = tile / 20 * width * 8;
        index += (tile % 20) * 8;
        index += ((pixel % 64) / 8) * width;
        index += (pixel % 8);
        index /= 4;

        pixelData[index] = 0;
        pixelData[index + 1] = 0;
        for(int j = 0; j < 2; j++) {
            pixelData[index] |= (((b1 >> j >> 4) & 1) | (((b2 >> j >> 4) & 1) << 1)) << (j * 4 + 8);
            pixelData[index] |= (((b1 >> j >> 6) & 1) | (((b2 >> j >> 6) & 1) << 1)) << (j * 4);
            pixelData[index + 1] |= (((b1 >> j) & 1) | (((b2 >> j) & 1) << 1)) << (j * 4 + 8);
            pixelData[index + 1] |= (((b1 >> j >> 2) & 1) | (((b2 >> j >> 2) & 1) << 1)) << (j * 4);
        }
    }

    if(appending) {
        std::fstream stream(filename, std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::ate);
        s64 end = stream.tellg();

        int temp = 0;

        // Update height
        stream.seekg(0x16);
        stream.read((char*) &temp, sizeof(temp));
        temp = -(height + (-temp));
        stream.seekg(0x16);
        stream.write((char*) &temp, sizeof(temp));

        // Update pixelArraySize
        stream.seekg(0x22);
        stream.read((char*) &temp, sizeof(temp));
        temp += pixelArraySize;
        stream.seekg(0x22);
        stream.write((char*) &temp, sizeof(temp));

        // Update file size
        temp += sizeof(bmpHeader);
        stream.seekg(0x2);
        stream.write((char*) &temp, sizeof(temp));

        // Append pixel data
        stream.seekg(end);
        stream.write((char*) pixelData, pixelArraySize);

        stream.close();
    } else { // Not appending; making a file from scratch
        std::fstream stream(filename, std::fstream::out | std::fstream::binary);

        *(u32*) (bmpHeader + 0x02) = sizeof(bmpHeader) + pixelArraySize;
        *(u32*) (bmpHeader + 0x22) = (u32) pixelArraySize;
        *(u32*) (bmpHeader + 0x12) = (u32) width;
        *(u32*) (bmpHeader + 0x16) = (u32) -height;

        stream.write((char*) bmpHeader, sizeof(bmpHeader));
        stream.write((char*) pixelData, pixelArraySize);
        stream.close();
    }

    free(pixelData);
}

#endif