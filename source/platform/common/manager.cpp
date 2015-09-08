#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include <sstream>

#include "platform/common/lodepng/lodepng.h"
#include "platform/common/config.h"
#include "platform/common/filechooser.h"
#include "platform/common/gbsplayer.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"
#include "gameboy.h"
#include "ppu.h"
#include "romfile.h"

Gameboy* gameboy = NULL;

int fps;
time_t lastPrintTime;

FileChooser romChooser("/", {"gbs", "sgb", "gbc", "cgb", "gb"}, true);
bool chooserInitialized = false;

void mgrInit() {
    gameboy = new Gameboy();
    fps = 0;
    lastPrintTime = 0;
}

void mgrExit() {
    if(gameboy) {
        delete gameboy;
        gameboy = NULL;
    }
}

void mgrLoadRom(const char* filename) {
    if(gameboy == NULL) {
        return;
    }

    if(!gameboy->loadRomFile(filename)) {
        uiClear();
        uiPrint("Error opening %s.", filename);
        uiPrint("\n\nPlease restart GameYob.\n");
        uiFlush();

        while(true) {
            systemCheckRunning();
            uiWaitForVBlank();
        }
    }

    gameboy->loadSave();

    // Border probing is broken
#if 0
    if(sgbBordersEnabled) {
        probingForBorder = true; // This will be ignored if starting in sgb mode, or if there is no sgb mode.
    }
#endif

    gameboy->getPPU()->sgbBorderLoaded = false; // Effectively unloads any sgb border
    mgrRefreshBorder();

    gameboy->init();
    if(gameboy->getRomFile()->isGBS()) {
        gbsPlayerReset();
        gbsPlayerDraw();
    } else if(mgrStateExists(-1)) {
        mgrLoadState(-1);
        mgrDeleteState(-1);
    }

    if(gameboy->getRomFile()->isGBS()) {
        disableMenuOption("Suspend");
        disableMenuOption("ROM Info");
        disableMenuOption("State Slot");
        disableMenuOption("Save State");
        disableMenuOption("Load State");
        disableMenuOption("Delete State");
        disableMenuOption("Manage Cheats");
        disableMenuOption("Accelerometer Pad");
        disableMenuOption("Exit without saving");
    } else {
        enableMenuOption("Suspend");
        enableMenuOption("ROM Info");
        enableMenuOption("State Slot");
        enableMenuOption("Save State");
        if(mgrStateExists(stateNum)) {
            enableMenuOption("Load State");
            enableMenuOption("Delete State");
        } else {
            disableMenuOption("Load State");
            disableMenuOption("Delete State");
        }

        enableMenuOption("Manage Cheats");

        if(gameboy->isRomLoaded() && gameboy->getRomFile()->getMBC() == MBC7) {
            enableMenuOption("Accelerometer Pad");
        } else {
            disableMenuOption("Accelerometer Pad");
        }

        if(gameboy->isRomLoaded() && gameboy->getRomFile()->getRamBanks() > 0 && !gameboy->autosaveEnabled) {
            enableMenuOption("Exit without saving");
        } else {
            disableMenuOption("Exit without saving");
        }
    }
}

void mgrUnloadRom() {
    if(gameboy == NULL || !gameboy->isRomLoaded()) {
        return;
    }

    gameboy->unloadRom();
    gameboy->linkedGameboy = NULL;

    gfxLoadBorder(NULL, 0, 0);
    gfxDrawScreen();
}

void mgrSelectRom() {
    mgrUnloadRom();

    if(!chooserInitialized) {
        chooserInitialized = true;

        DIR* dir = opendir(romPath.c_str());
        if(dir) {
            closedir(dir);
            romChooser.setDirectory(romPath);
        }
    }

    char* filename = romChooser.startFileChooser();
    if(filename == NULL) {
        systemExit();
    }

    mgrLoadRom(filename);
    free(filename);
}

int mgrReadBmp(u8** data, u32* width, u32* height, const char* filename) {
    FILE* fd = fopen(filename, "rb");
    if(!fd) {
        return 1;
    }

    char identifier[2];
    fread(identifier, 2, 1, fd);
    if(identifier[0] != 'B' || identifier[1] != 'M') {
        return 1;
    }

    u16 dataOffset;
    fseek(fd, 10, SEEK_SET);
    fread(&dataOffset, 2, 1, fd);

    u16 w;
    fseek(fd, 18, SEEK_SET);
    fread(&w, 2, 1, fd);

    u16 h;
    fseek(fd, 22, SEEK_SET);
    fread(&h, 2, 1, fd);

    u16 bits;
    fseek(fd, 28, SEEK_SET);
    fread(&bits, 2, 1, fd);
    u32 bytes = (u32) (bits / 8);

    u16 compression;
    fseek(fd, 30, SEEK_SET);
    fread(&compression, 2, 1, fd);

    size_t srcSize = (size_t) (w * h * bytes);
    u8* srcPixels = (u8*) malloc(srcSize);
    fseek(fd, dataOffset, SEEK_SET);
    fread(srcPixels, 1, srcSize, fd);

    u8* dstPixels = (u8*) malloc(w * h * sizeof(u32));

    if(bits == 16) {
        u16* srcPixels16 = (u16*) srcPixels;
        for(u32 x = 0; x < w; x++) {
            for(u32 y = 0; y < h; y++) {
                u32 srcPos = (h - y - 1) * w + x;
                u32 dstPos = (y * w + x) * sizeof(u32);

                u16 src = srcPixels16[srcPos];
                dstPixels[dstPos + 0] = 0xFF;
                dstPixels[dstPos + 1] = (u8) ((src & 0x1F) << 3);
                dstPixels[dstPos + 2] = (u8) (((src >> 5) & 0x1F) << 3);
                dstPixels[dstPos + 3] = (u8) (((src >> 10) & 0x1F) << 3);
            }
        }
    } else if(bits == 24 || bits == 32) {
        for(u32 x = 0; x < w; x++) {
            for(u32 y = 0; y < h; y++) {
                u32 srcPos = ((h - y - 1) * w + x) * bytes;
                u32 dstPos = (y * w + x) * sizeof(u32);

                dstPixels[dstPos + 0] = 0xFF;
                dstPixels[dstPos + 1] = srcPixels[srcPos + bytes - 3];
                dstPixels[dstPos + 2] = srcPixels[srcPos + bytes - 2];
                dstPixels[dstPos + 3] = srcPixels[srcPos + bytes - 1];
            }
        }
    } else {
        return 1;
    }

    free(srcPixels);

    *data = dstPixels;
    *width = w;
    *height = h;

    return 0;
}

int mgrReadPng(u8** data, u32* width, u32* height, const char* filename) {
    unsigned char* srcPixels;
    unsigned int w;
    unsigned int h;
    int lodeRet = lodepng_decode32_file(&srcPixels, &w, &h, filename);
    if(lodeRet != 0) {
        return lodeRet;
    }

    u8* dstPixels = (u8*) malloc(w * h * sizeof(u32));
    for(u32 x = 0; x < w; x++) {
        for(u32 y = 0; y < h; y++) {
            u32 src = (y * w + x) * 4;
            dstPixels[src + 0] = srcPixels[src + 3];
            dstPixels[src + 1] = srcPixels[src + 2];
            dstPixels[src + 2] = srcPixels[src + 1];
            dstPixels[src + 3] = srcPixels[src + 0];
        }
    }

    free(srcPixels);

    *data = dstPixels;
    *width = w;
    *height = h;

    return 0;
}

void mgrLoadBorderFile(const char* filename) {
    // Determine the file extension.
    const std::string path = filename;
    std::string::size_type dotPos = path.rfind('.');
    if(dotPos == std::string::npos) {
        return;
    }

    const std::string extension = path.substr(dotPos + 1);

    // Load the image.
    u8* imgData;
    u32 imgWidth;
    u32 imgHeight;
    if((strcasecmp(extension.c_str(), "png") == 0 && mgrReadPng(&imgData, &imgWidth, &imgHeight, filename) == 0) || (strcasecmp(extension.c_str(), "bmp") == 0 && mgrReadBmp(&imgData, &imgWidth, &imgHeight, filename) == 0)) {
        gfxLoadBorder(imgData, imgWidth, imgHeight);
        free(imgData);
    }
}

bool mgrTryRawBorderFile(std::string border) {
    FILE* file = fopen(border.c_str(), "r");
    if(file != NULL) {
        fclose(file);
        mgrLoadBorderFile(border.c_str());
        return true;
    }

    return false;
}

static const char* scaleNames[] = {"off", "125", "150", "aspect", "full"};

bool mgrTryBorderFile(std::string border) {
    std::string extension = "";
    std::string::size_type dotPos = border.rfind('.');
    if(dotPos != std::string::npos) {
        extension = border.substr(dotPos);
        border = border.substr(0, dotPos);
    }

    return (borderScaleMode == 0 && mgrTryRawBorderFile(border + "_" + scaleNames[scaleMode] + extension)) || mgrTryRawBorderFile(border + extension);
}

bool mgrTryBorderName(std::string border) {
    return mgrTryBorderFile(border + ".png") || mgrTryBorderFile(border + ".bmp");
}

void mgrRefreshBorder() {
    // TODO: SGB?

    if(borderSetting == 1) {
        if((gameboy->isRomLoaded() && mgrTryBorderName(gameboy->getRomFile()->getFileName())) || mgrTryBorderFile(borderPath)) {
            return;
        }
    }

    gfxLoadBorder(NULL, 0, 0);
}

void mgrRefreshBios() {
    gameboy->gbBiosLoaded = false;
    gameboy->gbcBiosLoaded = false;

    FILE* gbFile = fopen(gbBiosPath.c_str(), "rb");
    if(gbFile != NULL) {
        struct stat st;
        fstat(fileno(gbFile), &st);

        if(st.st_size == 0x100) {
            gameboy->gbBiosLoaded = true;
            fread(gameboy->gbBios, 1, 0x100, gbFile);
        }

        fclose(gbFile);
    }

    FILE* gbcFile = fopen(gbcBiosPath.c_str(), "rb");
    if(gbcFile != NULL) {
        struct stat st;
        fstat(fileno(gbcFile), &st);

        if(st.st_size == 0x900) {
            gameboy->gbcBiosLoaded = true;
            fread(gameboy->gbcBios, 1, 0x900, gbcFile);
        }

        fclose(gbFile);
    }
}

const std::string mgrGetStateName(int stateNum) {
    std::stringstream nameStream;
    if(stateNum == -1) {
        nameStream << gameboy->getRomFile()->getFileName() << ".yss";
    } else {
        nameStream << gameboy->getRomFile()->getFileName() << ".ys" << stateNum;
    }

    return nameStream.str();
}

bool mgrStateExists(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return false;
    }

    FILE* file = fopen(mgrGetStateName(stateNum).c_str(), "r");
    if(file != NULL) {
        fclose(file);
    }

    return file != NULL;
}

bool mgrLoadState(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return false;
    }

    FILE* file = fopen(mgrGetStateName(stateNum).c_str(), "r");
    if(file == NULL) {
        return false;
    }

    bool ret = gameboy->loadState(file);
    fclose(file);
    return ret;
}

bool mgrSaveState(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return false;
    }

    FILE* file = fopen(mgrGetStateName(stateNum).c_str(), "w");
    if(file == NULL) {
        return false;
    }

    bool ret = gameboy->saveState(file);
    fclose(file);
    return ret;
}

void mgrDeleteState(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return;
    }

    remove(mgrGetStateName(stateNum).c_str());
}

void mgrSave() {
    if(gameboy == NULL || !gameboy->isRomLoaded()) {
        return;
    }

    gameboy->saveGame();
}

void mgrRun() {
    systemCheckRunning();

    if(gameboy == NULL || !gameboy->isRomLoaded()) {
        return;
    }

    while(!(gameboy->runEmul() & RET_VBLANK));

    gameboy->getPPU()->drawScreen();

    inputUpdate();

    if(isMenuOn()) {
        updateMenu();
    } else {
        if(gameboy->getRomFile()->isGBS()) {
            gbsPlayerRefresh();
        }

        gameboy->gameboyCheckInput();
        if(inputKeyPressed(FUNC_KEY_SAVE)) {
            if(!gameboy->autosaveEnabled) {
                gameboy->saveGame();
            }
        }

        if(inputKeyPressed(FUNC_KEY_FAST_FORWARD_TOGGLE)) {
            gfxToggleFastForward();
        }

        if((inputKeyPressed(FUNC_KEY_MENU) || inputKeyPressed(FUNC_KEY_MENU_PAUSE)) && !accelPadMode) {
            if(pauseOnMenu || inputKeyPressed(FUNC_KEY_MENU_PAUSE)) {
                gameboy->pause();
            }

            gfxSetFastForward(false);
            displayMenu();
        }

        if(inputKeyPressed(FUNC_KEY_SCALE)) {
            setMenuOption("Scaling", (getMenuOption("Scaling") + 1) % 5);
        }

        if(inputKeyPressed(FUNC_KEY_RESET)) {
            gameboy->init();

            if(gameboy->isRomLoaded() && gameboy->getRomFile()->isGBS()) {
                gbsPlayerReset();
                gbsPlayerDraw();
            }
        }
    }

    time_t rawTime = 0;
    time(&rawTime);
    fps++;
    if(rawTime > lastPrintTime) {
        if(!isMenuOn() && !showConsoleDebug() && (!gameboy->isRomLoaded() || !gameboy->getRomFile()->isGBS()) && (fpsOutput || timeOutput)) {
            uiClear();
            int fpsLength = 0;
            if(fpsOutput) {
                char buffer[16];
                snprintf(buffer, 16, "FPS: %d", fps);
                uiPrint("%s", buffer);
                fpsLength = (int) strlen(buffer);
            }

            if(timeOutput) {
                char *timeString = ctime(&rawTime);
                for(int i = 0; ; i++) {
                    if(timeString[i] == ':') {
                        timeString += i - 2;
                        break;
                    }
                }

                char timeDisplay[6] = {0};
                strncpy(timeDisplay, timeString, 5);

                int spaces = uiGetWidth() - (int) strlen(timeDisplay) - fpsLength;
                for(int i = 0; i < spaces; i++) {
                    uiPrint(" ");
                }

                uiPrint("%s", timeDisplay);
            }

            uiPrint("\n");

            uiFlush();
        }

        fps = 0;
        lastPrintTime = rawTime;
    }
}
