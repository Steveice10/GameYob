#ifdef BACKEND_SWITCH

#include <string.h>
#include <unistd.h>

#include <fstream>

#include <switch.h>

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

static int numPrinted;

bool systemInit(int argc, char* argv[]) {
    if(!gfxInit()) {
        return false;
    }

    audioInit();
    uiInit();
    inputInit();

    requestedExit = false;

    numPrinted = 0;

    return true;
}

void systemExit() {
    inputCleanup();
    uiCleanup();
    audioCleanup();
    gfxCleanup();
}

bool systemIsRunning() {
    return !requestedExit && appletMainLoop();
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
    return false; // TODO
}

void systemSetIRState(bool state) {
    // TODO
}

const std::string systemGetIP() {
    return ""; // TODO
}

int systemSocketListen(u16 port) {
    return -1; // TODO
}

FILE* systemSocketAccept(int listeningSocket, std::string* acceptedIp) {
    return NULL; // TODO
}

FILE* systemSocketConnect(const std::string ipAddress, u16 port) {
    return NULL; // TODO
}

void systemSetRumble(bool rumble) {
    // TODO
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