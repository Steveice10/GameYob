#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include <sstream>

#include "platform/common/picopng.h"
#include "platform/common/cheatengine.h"
#include "platform/common/config.h"
#include "platform/common/filechooser.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"
#include "gameboy.h"
#include "mbc.h"
#include "ppu.h"
#include "romfile.h"
#include "sgb.h"

Gameboy* gameboy = NULL;
CheatEngine* cheatEngine = NULL;

u8 gbBios[0x200];
bool gbBiosLoaded;
u8 gbcBios[0x900];
bool gbcBiosLoaded;

int fps;
time_t lastPrintTime;

int fastForwardCounter;

int autoFireCounterA;
int autoFireCounterB;

time_t lastSaveTime;

bool emulationPaused;

FileChooser romChooser("/", {"sgb", "gbc", "cgb", "gb"}, true);
bool chooserInitialized = false;

void mgrInit() {
    gameboy = new Gameboy();
    cheatEngine = new CheatEngine(gameboy);

    memset(gbBios, 0, sizeof(gbBios));
    gbBiosLoaded = false;
    memset(gbcBios, 0, sizeof(gbcBios));
    gbcBiosLoaded = false;

    fps = 0;
    lastPrintTime = 0;

    fastForwardCounter = 0;

    autoFireCounterA = 0;
    autoFireCounterB = 0;

    lastSaveTime = 0;

    emulationPaused = false;

    chooserInitialized = false;
}

void mgrExit() {
    if(gameboy != NULL) {
        delete gameboy;
        gameboy = NULL;
    }

    if(cheatEngine != NULL) {
        delete cheatEngine;
        cheatEngine = NULL;
    }
}

void mgrLoadRom(const char* filename) {
    if(gameboy == NULL || !gameboy->loadRomFile(filename)) {
        return;
    }

    cheatEngine->loadCheats((gameboy->romFile->getFileName() + ".cht").c_str());

    mgrRefreshBorder();

    gameboy->reset();
    if(mgrStateExists(-1)) {
        mgrLoadState(-1);
        mgrDeleteState(-1);
    }

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

    if(gameboy->isRomLoaded() && gameboy->romFile->getMBCType() == MBC7) {
        enableMenuOption("Accelerometer Pad");
    } else {
        disableMenuOption("Accelerometer Pad");
    }

    if(gameboy->isRomLoaded() && gameboy->romFile->getRamBanks() > 0 && !autoSaveEnabled) {
        enableMenuOption("Exit without saving");
    } else {
        disableMenuOption("Exit without saving");
    }
}

void mgrUnloadRom() {
    gfxClearScreenBuffer(0x0000);
    gfxLoadBorder(NULL, 0, 0);
    gfxDrawScreen();

    if(gameboy == NULL || !gameboy->isRomLoaded()) {
        return;
    }

    gameboy->unloadRom();
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

    char* filename = romChooser.chooseFile();
    if(filename == NULL) {
        systemRequestExit();
        return;
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
                u32 dstPos = (u32) ((y * w + x) * sizeof(u32));

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
                u32 dstPos = (u32) ((y * w + x) * sizeof(u32));

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
    FILE* fd = fopen(filename, "rb");
    if(fd == NULL) {
        return errno;
    }

    struct stat st;
    fstat(fileno(fd), &st);

    u8* buf = new u8[st.st_size];
    int readRes = fread(buf, 1, (size_t) st.st_size, fd);
    fclose(fd);
    if(readRes < 0) {
        return errno;
    }

    std::vector<u8> srcPixels;
    u32 w;
    u32 h;
    int lodeRet = decodePNG(srcPixels, w, h, buf, (size_t) st.st_size);
    delete buf;
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
        if((gameboy->isRomLoaded() && mgrTryBorderName(gameboy->romFile->getFileName())) || mgrTryBorderFile(borderPath)) {
            return;
        }
    }

    gfxLoadBorder(NULL, 0, 0);
}

void mgrRefreshBios() {
    gbBiosLoaded = false;
    gbcBiosLoaded = false;

    FILE* gbFile = fopen(gbBiosPath.c_str(), "rb");
    if(gbFile != NULL) {
        struct stat st;
        fstat(fileno(gbFile), &st);

        if(st.st_size == 0x100) {
            gbBiosLoaded = true;
            fread(gbBios, 1, 0x100, gbFile);
        }

        fclose(gbFile);
    }

    FILE* gbcFile = fopen(gbcBiosPath.c_str(), "rb");
    if(gbcFile != NULL) {
        struct stat st;
        fstat(fileno(gbcFile), &st);

        if(st.st_size == 0x900) {
            gbcBiosLoaded = true;
            fread(gbcBios, 1, 0x900, gbcFile);
        }

        fclose(gbFile);
    }
}

const std::string mgrGetStateName(int stateNum) {
    std::stringstream nameStream;
    if(stateNum == -1) {
        nameStream << gameboy->romFile->getFileName() << ".yss";
    } else {
        nameStream << gameboy->romFile->getFileName() << ".ys" << stateNum;
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

    gameboy->mbc->save();
}

void mgrPause() {
    emulationPaused = true;
}

void mgrUnpause() {
    emulationPaused = false;
}

bool mgrIsPaused() {
    return emulationPaused;
}

void mgrRun() {
    if(gameboy == NULL) {
        return;
    }

    while(!gameboy->isRomLoaded()) {
        if(!systemIsRunning()) {
            return;
        }

        mgrSelectRom();
    }

    inputUpdate();

    if(isMenuOn()) {
        updateMenu();
    } else if(gameboy->isRomLoaded()) {
        u8 buttonsPressed = 0xFF;

        if(inputKeyHeld(FUNC_KEY_UP)) {
            buttonsPressed &= ~GB_UP;
        }

        if(inputKeyHeld(FUNC_KEY_DOWN)) {
            buttonsPressed &= ~GB_DOWN;
        }

        if(inputKeyHeld(FUNC_KEY_LEFT)) {
            buttonsPressed &= ~GB_LEFT;
        }

        if(inputKeyHeld(FUNC_KEY_RIGHT)) {
            buttonsPressed &= ~GB_RIGHT;
        }

        if(inputKeyHeld(FUNC_KEY_A)) {
            buttonsPressed &= ~GB_A;
        }

        if(inputKeyHeld(FUNC_KEY_B)) {
            buttonsPressed &= ~GB_B;
        }

        if(inputKeyHeld(FUNC_KEY_START)) {
            buttonsPressed &= ~GB_START;
        }

        if(inputKeyHeld(FUNC_KEY_SELECT)) {
            buttonsPressed &= ~GB_SELECT;
        }

        if(inputKeyHeld(FUNC_KEY_AUTO_A)) {
            if(autoFireCounterA <= 0) {
                buttonsPressed &= ~GB_A;
                autoFireCounterA = 2;
            }

            autoFireCounterA--;
        }

        if(inputKeyHeld(FUNC_KEY_AUTO_B)) {
            if(autoFireCounterB <= 0) {
                buttonsPressed &= ~GB_B;
                autoFireCounterB = 2;
            }

            autoFireCounterB--;
        }

        gameboy->sgb->setController(0, buttonsPressed);

        if(inputKeyPressed(FUNC_KEY_SAVE)) {
            if(!autoSaveEnabled) {
                mgrSave();
            }
        }

        if(inputKeyPressed(FUNC_KEY_FAST_FORWARD_TOGGLE)) {
            gfxToggleFastForward();
        }

        if((inputKeyPressed(FUNC_KEY_MENU) || inputKeyPressed(FUNC_KEY_MENU_PAUSE))) {
            if(pauseOnMenu || inputKeyPressed(FUNC_KEY_MENU_PAUSE)) {
                mgrPause();
            }

            gfxSetFastForward(false);
            displayMenu();
        }

        if(inputKeyPressed(FUNC_KEY_SCALE)) {
            setMenuOption("Scaling", (getMenuOption("Scaling") + 1) % 5);
        }

        if(inputKeyPressed(FUNC_KEY_RESET)) {
            gameboy->reset();
        }
    }

    if(gameboy->isRomLoaded() && !emulationPaused) {
        while(!(gameboy->run() & RET_VBLANK));

        cheatEngine->applyGSCheats();

        if(!gfxGetFastForward() || fastForwardCounter++ >= fastForwardFrameSkip) {
            fastForwardCounter = 0;
            gfxDrawScreen();
        }
    }

    time_t rawTime = 0;
    time(&rawTime);

    if(autoSaveEnabled && rawTime > lastSaveTime + 60) {
        mgrSave();
        lastSaveTime = rawTime;
    }

    fps++;
    if(rawTime > lastPrintTime) {
        if(!isMenuOn() && !showConsoleDebug() && (fpsOutput || timeOutput)) {
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
