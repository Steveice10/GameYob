#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <sstream>

#include "platform/common/cheatengine.h"
#include "platform/common/config.h"
#include "platform/common/filechooser.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/common/stb_image.h"
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

int fastForwardCounter;

static std::string romName;

static int fps;
static time_t lastPrintTime;

static int autoFireCounterA;
static int autoFireCounterB;

static bool emulationPaused;

static FileChooser romChooser("/", {"sgb", "gbc", "cgb", "gb"}, true);
static bool chooserInitialized = false;

void mgrInit() {
    gameboy = new Gameboy();
    cheatEngine = new CheatEngine(gameboy);

    memset(gbBios, 0, sizeof(gbBios));
    gbBiosLoaded = false;
    memset(gbcBios, 0, sizeof(gbcBios));
    gbcBiosLoaded = false;

    fastForwardCounter = 0;

    romName = "";

    fps = 0;
    lastPrintTime = 0;

    autoFireCounterA = 0;
    autoFireCounterB = 0;

    emulationPaused = false;

    chooserInitialized = false;
}

void mgrExit() {
    mgrUnloadRom();

    if(gameboy != NULL) {
        delete gameboy;
        gameboy = NULL;
    }

    if(cheatEngine != NULL) {
        delete cheatEngine;
        cheatEngine = NULL;
    }
}

void mgrReset(bool initial) {
    if(gameboy == NULL || !gameboy->isRomLoaded()) {
        return;
    }

    if(!initial) {
        mgrWriteSave();
    }

    gameboy->reset();
    mgrLoadSave();
}

std::string mgrGetRomName() {
    return romName;
}

void mgrLoadRom(const char* filename) {
    if(gameboy == NULL || filename == NULL) {
        return;
    }

    mgrUnloadRom();

    std::ifstream stream(filename, std::ios::binary | std::ios::ate);
    if(!stream.is_open()) {
        systemPrintDebug("Failed to open ROM file: %s\n", strerror(errno));
        return;
    }

    int size = (int) stream.tellg();
    stream.seekg(0);

    bool result = gameboy->loadRom(stream, size);
    stream.close();

    if(!result) {
        return;
    }

    romName = filename;
    std::string::size_type dot = romName.find_last_of('.');
    if(dot != std::string::npos) {
        romName = romName.substr(0, dot);
    }

    cheatEngine->loadCheats((romName + ".cht").c_str());

    mgrReset(true);
    if(mgrStateExists(-1)) {
        mgrLoadState(-1);
        mgrDeleteState(-1);
    }

    mgrRefreshBorder();

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

    if(gameboy->isRomLoaded() && gameboy->romFile->getRamBanks() > 0) {
        enableMenuOption("Exit without saving");
    } else {
        disableMenuOption("Exit without saving");
    }
}

void mgrUnloadRom(bool save) {
    if(gameboy != NULL && gameboy->isRomLoaded()) {
        if(save) {
            mgrWriteSave();
        }

        gameboy->unloadRom();
    }

    romName = "";

    mgrRefreshBorder();

    gfxClearScreenBuffer(0x0000);
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

    char* filename = romChooser.chooseFile();
    if(filename == NULL) {
        systemRequestExit();
        return;
    }

    mgrLoadRom(filename);
    free(filename);
}

void mgrLoadSave() {
    if(gameboy == NULL || !gameboy->isRomLoaded() || gameboy->romFile->getRamBanks() == 0) {
        return;
    }

    std::ifstream stream(romName + ".sav", std::ios::binary);
    if(!stream.is_open()) {
        systemPrintDebug("Failed to open save file: %s\n", strerror(errno));
        return;
    }

    gameboy->mbc->load(stream);
    stream.close();
}

void mgrWriteSave() {
    if(gameboy == NULL || !gameboy->isRomLoaded() || gameboy->romFile->getRamBanks() == 0) {
        return;
    }

    std::ofstream stream(romName + ".sav", std::ios::binary);
    if(!stream.is_open()) {
        systemPrintDebug("Failed to open save file: %s\n", strerror(errno));
        return;
    }

    gameboy->mbc->save(stream);
    stream.close();
}

const std::string mgrGetStateName(int stateNum) {
    std::stringstream nameStream;
    if(stateNum == -1) {
        nameStream << mgrGetRomName() << ".yss";
    } else {
        nameStream << mgrGetRomName() << ".ys" << stateNum;
    }

    return nameStream.str();
}

bool mgrStateExists(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return false;
    }

    std::ifstream stream(mgrGetStateName(stateNum), std::ios::binary);
    if(stream.is_open()) {
        stream.close();
        return true;
    }

    return false;
}

bool mgrLoadState(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return false;
    }

    std::ifstream stream(mgrGetStateName(stateNum), std::ios::binary);
    if(!stream.is_open()) {
        systemPrintDebug("Failed to open state file: %s\n", strerror(errno));
        return false;
    }

    bool ret = gameboy->loadState(stream);
    stream.close();
    return ret;
}

bool mgrSaveState(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return false;
    }

    std::ofstream stream(mgrGetStateName(stateNum), std::ios::binary);
    if(!stream.is_open()) {
        systemPrintDebug("Failed to open state file: %s\n", strerror(errno));
        return false;
    }

    bool ret = gameboy->saveState(stream);
    stream.close();
    return ret;
}

void mgrDeleteState(int stateNum) {
    if(!gameboy->isRomLoaded()) {
        return;
    }

    remove(mgrGetStateName(stateNum).c_str());
}

bool mgrTryRawBorderFile(std::string border) {
    std::ifstream stream(border, std::ios::binary);
    if(stream.is_open()) {
        stream.close();

        int imgWidth;
        int imgHeight;
        int imgDepth;
        u8* image = stbi_load(border.c_str(), &imgWidth, &imgHeight, &imgDepth, STBI_rgb_alpha);
        if(image == NULL || imgDepth != STBI_rgb_alpha) {
            if(image != NULL) {
                stbi_image_free(image);
            }

            systemPrintDebug("Failed to decode image file.\n");
            return false;
        }

        u8* imgData = new u8[imgWidth * imgHeight * sizeof(u32)];
        for(int x = 0; x < imgWidth; x++) {
            for(int y = 0; y < imgHeight; y++) {
                int offset = (y * imgWidth + x) * 4;
                imgData[offset + 0] = image[offset + 3];
                imgData[offset + 1] = image[offset + 2];
                imgData[offset + 2] = image[offset + 1];
                imgData[offset + 3] = image[offset + 0];
            }
        }

        stbi_image_free(image);

        gfxLoadBorder(imgData, imgWidth, imgHeight);
        delete imgData;

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
    for(std::string extension : supportedImages) {
        if(mgrTryBorderFile(border + "." + extension)) {
            return true;
        }
    }

    return false;
}

bool mgrTryCustomBorder() {
    return gameboy->isRomLoaded() && (mgrTryBorderName(mgrGetRomName()) || mgrTryBorderFile(borderPath));
}

bool mgrTrySgbBorder() {
    if(gameboy->isRomLoaded() && gameboy->gbMode == MODE_SGB && gameboy->sgb->getBg() != NULL) {
        gfxLoadBorder((u8*) gameboy->sgb->getBg(), 256, 224);
        return true;
    }

    return false;
}

void mgrRefreshBorder() {
    switch(borderSetting) {
        case 1:
            if(mgrTrySgbBorder()) {
                return;
            }

            break;
        case 2:
            if(mgrTryCustomBorder()) {
                return;
            }

            break;
        case 3:
            if(mgrTrySgbBorder() || mgrTryCustomBorder()) {
                return;
            }

            break;
        case 4:
            if(mgrTryCustomBorder() || mgrTrySgbBorder()) {
                return;
            }

            break;
        default:
            break;
    }

    gfxLoadBorder(NULL, 0, 0);
}

void mgrRefreshBios() {
    gbBiosLoaded = false;
    gbcBiosLoaded = false;

    std::ifstream gbStream(gbBiosPath, std::ios::binary);
    if(gbStream.is_open()) {
        gbStream.read((char*) gbBios, sizeof(gbBios));
        gbStream.close();

        gbBiosLoaded = true;
    }

    std::ifstream gbcStream(gbcBiosPath, std::ios::binary);
    if(gbcStream.is_open()) {
        gbcStream.read((char*) gbcBios, sizeof(gbcBios));
        gbcStream.close();

        gbcBiosLoaded = true;
    }
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
            mgrWriteSave();
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
            mgrReset();
        }

        if(inputKeyPressed(FUNC_KEY_SCREENSHOT) && !isMenuOn()) {
            gfxTakeScreenshot();
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

    fps++;

    time_t rawTime = 0;
    time(&rawTime);

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
