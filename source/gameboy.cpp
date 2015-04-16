#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gbcpalette.h"
#include "gbprinter.h"
#include "gameboy.h"
#include "soundengine.h"
#include "inputhelper.h"
#include "console.h"
#include "cheats.h"
#include "gbs.h"
#include "menu.h"
#include "gbmanager.h"

#include <ctrcommon/fs.hpp>
#include <ctrcommon/input.hpp>
#include <gfx.h>

const int maxWaitCycles = 1000000;

Gameboy::Gameboy() : hram(highram + 0xe00), ioRam(highram + 0xf00) {
    saveFile = NULL;

    romFile = NULL;

    fpsOutput = true;
    timeOutput = true;

    cyclesSinceVBlank = 0;
    probingForBorder = false;

    // private
    resettingGameboy = false;
    wroteToSramThisFrame = false;
    framesSinceAutosaveStarted = 0;

    externRam = NULL;
    saveModified = false;
    numSaveWrites = 0;
    autosaveStarted = false;

    cheatEngine = new CheatEngine(this);
    soundEngine = new SoundEngine(this);
    if(this != gameboy)
        soundEngine->mute();
}

Gameboy::~Gameboy() {
    unloadRom();

    delete cheatEngine;
    delete soundEngine;
}

void Gameboy::init() {
    enableSleepMode();

    if(gbsMode) {
        resultantGBMode = 1; // GBC
        probingForBorder = false;
    }
    else {
        switch(gbcModeOption) {
            case 0: // GB
                initGBMode();
                break;
            case 1: // GBC if needed
                if(romFile->romSlot0[0x143] == 0xC0)
                    initGBCMode();
                else
                    initGBMode();
                break;
            case 2: // GBC
                if(romFile->romSlot0[0x143] == 0x80 || romFile->romSlot0[0x143] == 0xC0)
                    initGBCMode();
                else
                    initGBMode();
                break;
        }

        bool sgbEnhanced = romFile->romSlot0[0x14b] == 0x33 && romFile->romSlot0[0x146] == 0x03;
        if(sgbEnhanced && resultantGBMode != 2 && probingForBorder) {
            resultantGBMode = 2;
        }
        else {
            probingForBorder = false;
        }
    } // !gbsMode

    sgbMode = false;

    initMMU();
    initCPU();
    sgbInit();

    cyclesSinceVBlank = 0;
    gameboyFrameCounter = 0;

    gameboyPaused = false;
    setDoubleSpeed(0);

    scanlineCounter = 456 * (doubleSpeed ? 2 : 1);
    phaseCounter = 456 * 153;
    timerCounter = 0;
    dividerCounter = 256;
    serialCounter = 0;

    cyclesToEvent = 0;
    extraCycles = 0;
    soundCycles = 0;
    cyclesSinceVBlank = 0;
    cycleToSerialTransfer = -1;

    initGbPrinter();

    // Timer stuff
    periods[0] = clockSpeed / 4096;
    periods[1] = clockSpeed / 262144;
    periods[2] = clockSpeed / 65536;
    periods[3] = clockSpeed / 16384;
    timerPeriod = periods[0];

    memset(vram[0], 0, 0x2000);
    memset(vram[1], 0, 0x2000);

    initGFXPalette();
    if(isMainGameboy()) {
        initGFX();
    }
    initSND();

    if(!gbsMode && !probingForBorder && checkStateExists(-1)) {
        loadState(-1);
    }

    if(gbsMode)
        gbsInit();
}

void Gameboy::initGBMode() {
    if(sgbModeOption != 0 && romFile->romSlot0[0x14b] == 0x33 && romFile->romSlot0[0x146] == 0x03)
        resultantGBMode = 2;
    else {
        resultantGBMode = 0;
    }
}

void Gameboy::initGBCMode() {
    if(sgbModeOption == 2 && romFile->romSlot0[0x14b] == 0x33 && romFile->romSlot0[0x146] == 0x03)
        resultantGBMode = 2;
    else {
        resultantGBMode = 1;
    }
}

void Gameboy::initSND() {

    // Sound stuff
    for(int i = 0x27; i <= 0x2f; i++)
        ioRam[i] = 0xff;

    ioRam[0x26] = 0xf0;

    ioRam[0x10] = 0x80;
    ioRam[0x11] = 0xBF;
    ioRam[0x12] = 0xF3;
    ioRam[0x14] = 0xBF;
    ioRam[0x16] = 0x3F;
    ioRam[0x17] = 0x00;
    ioRam[0x19] = 0xbf;
    ioRam[0x1a] = 0x7f;
    ioRam[0x1b] = 0xff;
    ioRam[0x1c] = 0x9f;
    ioRam[0x1e] = 0xbf;
    ioRam[0x20] = 0xff;
    ioRam[0x21] = 0x00;
    ioRam[0x22] = 0x00;
    ioRam[0x23] = 0xbf;
    ioRam[0x24] = 0x77;
    ioRam[0x25] = 0xf3;

    // Initial values for the waveform are different depending on the model.
    // These values are the defaults for the GBC.
    for(int i = 0; i < 0x10; i += 2)
        ioRam[0x30 + i] = 0;
    for(int i = 1; i < 0x10; i += 2)
        ioRam[0x30 + i] = 0xff;

    soundEngine->cyclesToSoundEvent = 0;

    soundEngine->init();
}

// Called either from startup, or when FF50 is written to.
void Gameboy::initGameboyMode() {
    gbRegs.af.b.l = 0xB0;
    gbRegs.bc.w = 0x0013;
    gbRegs.de.w = 0x00D8;
    gbRegs.hl.w = 0x014D;
    switch(resultantGBMode) {
        case 0: // GB
            gbRegs.af.b.h = 0x01;
            gbMode = GB;
            if(romFile->romSlot0[0x143] == 0x80 || romFile->romSlot0[0x143] == 0xC0)
                // Init the palette in case it was overwritten, since it
                // assumed it was starting in GBC mode.
                initGFXPalette();
            break;
        case 1: // GBC
            gbRegs.af.b.h = 0x11;
            if(gbaModeOption)
                gbRegs.bc.b.h |= 1;
            gbMode = CGB;
            break;
        case 2: // SGB
            sgbMode = true;
            gbRegs.af.b.h = 0x01;
            gbMode = GB;
            break;
    }

    memcpy(&g_gbRegs, &gbRegs, sizeof(Registers));
}


void Gameboy::gameboyCheckInput() {
    static int autoFireCounterA = 0, autoFireCounterB = 0;

    u8 buttonsPressed = 0xff;

    if(probingForBorder)
        return;

    if(keyPressed(mapFuncKey(FUNC_KEY_UP))) {
        buttonsPressed &= (0xFF ^ GB_UP);
        if(!(ioRam[0x00] & 0x10))
            requestInterrupt(INT_JOYPAD);
    }
    if(keyPressed(mapFuncKey(FUNC_KEY_DOWN))) {
        buttonsPressed &= (0xFF ^ GB_DOWN);
        if(!(ioRam[0x00] & 0x10))
            requestInterrupt(INT_JOYPAD);
    }
    if(keyPressed(mapFuncKey(FUNC_KEY_LEFT))) {
        buttonsPressed &= (0xFF ^ GB_LEFT);
        if(!(ioRam[0x00] & 0x10))
            requestInterrupt(INT_JOYPAD);
    }
    if(keyPressed(mapFuncKey(FUNC_KEY_RIGHT))) {
        buttonsPressed &= (0xFF ^ GB_RIGHT);
        if(!(ioRam[0x00] & 0x10))
            requestInterrupt(INT_JOYPAD);
    }
    if(keyPressed(mapFuncKey(FUNC_KEY_A))) {
        buttonsPressed &= (0xFF ^ GB_A);
        if(!(ioRam[0x00] & 0x20))
            requestInterrupt(INT_JOYPAD);
    }
    if(keyPressed(mapFuncKey(FUNC_KEY_B))) {
        buttonsPressed &= (0xFF ^ GB_B);
        if(!(ioRam[0x00] & 0x20))
            requestInterrupt(INT_JOYPAD);
    }
    if(keyPressed(mapFuncKey(FUNC_KEY_START))) {
        buttonsPressed &= (0xFF ^ GB_START);
        if(!(ioRam[0x00] & 0x20))
            requestInterrupt(INT_JOYPAD);
    }
    if(keyPressed(mapFuncKey(FUNC_KEY_SELECT))) {
        buttonsPressed &= (0xFF ^ GB_SELECT);
        if(!(ioRam[0x00] & 0x20))
            requestInterrupt(INT_JOYPAD);
    }

    if(keyPressed(mapFuncKey(FUNC_KEY_AUTO_A))) {
        if(autoFireCounterA <= 0) {
            buttonsPressed &= (0xFF ^ GB_A);
            if(!(ioRam[0x00] & 0x20))
                requestInterrupt(INT_JOYPAD);
            autoFireCounterA = 2;
        }
        autoFireCounterA--;
    }
    if(keyPressed(mapFuncKey(FUNC_KEY_AUTO_B))) {
        if(autoFireCounterB <= 0) {
            buttonsPressed &= (0xFF ^ GB_B);
            if(!(ioRam[0x00] & 0x20))
                requestInterrupt(INT_JOYPAD);
            autoFireCounterB = 2;
        }
        autoFireCounterB--;
    }

    controllers[0] = buttonsPressed;

    if(keyJustPressed(mapFuncKey(FUNC_KEY_SAVE))) {
        if(!autoSavingEnabled) {
            saveGame();
        }
    }

    if(keyJustPressed(mapFuncKey(FUNC_KEY_FAST_FORWARD_TOGGLE))) {
        gfxToggleFastForward();
    }

    if(keyJustPressed(mapFuncKey(FUNC_KEY_MENU) | mapFuncKey(FUNC_KEY_MENU_PAUSE) | BUTTON_TOUCH) && !accelPadMode) {
        if(singleScreenMode || keyJustPressed(mapFuncKey(FUNC_KEY_MENU_PAUSE)))
            pause();

        forceReleaseKey(0xffffffff);
        gfxSetFastForward(false);
        displayMenu();
    }

    if(keyJustPressed(mapFuncKey(FUNC_KEY_SCALE))) {
        setMenuOption("Scaling", (getMenuOption("Scaling") + 1) % 3);
    }

    if(keyJustPressed(mapFuncKey(FUNC_KEY_RESET)))
        resetGameboy();
}

// This is called 60 times per gameboy second, even if the lcd is off.
void Gameboy::gameboyUpdateVBlank() {
    gameboyFrameCounter++;

    if(!gbsMode) {
        if(resettingGameboy) {
            init();
            resettingGameboy = false;
        }

        if(probingForBorder) {
            if(gameboyFrameCounter >= 450) {
                // Give up on finding a sgb border.
                probingForBorder = false;
                sgbBorderLoaded = false;
                init();
            }
            return;
        }

        updateAutosave();

        if(cheatEngine->areCheatsEnabled())
            cheatEngine->applyGSCheats();

        updateGbPrinter();
    }
}

// This function can be called from weird contexts, so just set a flag to deal 
// with it later.
void Gameboy::resetGameboy() {
    resettingGameboy = true;
}

void Gameboy::pause() {
    if(!gameboyPaused) {
        gameboyPaused = true;
        muteSND();
    }
}

void Gameboy::unpause() {
    if(gameboyPaused) {
        gameboyPaused = false;
        unmuteSND();
    }
}

bool Gameboy::isGameboyPaused() {
    return gameboyPaused;
}

int Gameboy::runEmul() {
    emuRet = 0;
    memcpy(&g_gbRegs, &gbRegs, sizeof(Registers));

    if(cycleToSerialTransfer != -1)
        setEventCycles(cycleToSerialTransfer);

    for(; ;) {
        cyclesToEvent -= extraCycles;
        int cycles;
        if(halt)
            cycles = cyclesToEvent;
        else
            cycles = runOpcode(cyclesToEvent);

        bool opTriggeredInterrupt = cyclesToExecute == -1;

        cycles += extraCycles;

        cyclesToEvent = maxWaitCycles;
        extraCycles = 0;

        cyclesSinceVBlank += cycles;

        // For external clock
        if(cycleToSerialTransfer != -1) {
            if(cyclesSinceVBlank < cycleToSerialTransfer)
                setEventCycles(cycleToSerialTransfer - cyclesSinceVBlank);
            else {
                cycleToSerialTransfer = -1;

                if((ioRam[0x02] & 0x81) == 0x80) {
                    u8 tmp = ioRam[0x01];
                    ioRam[0x01] = linkedGameboy->ioRam[0x01];
                    linkedGameboy->ioRam[0x01] = tmp;
                    emuRet |= RET_LINK;
                    // Execution will be passed back to the other gameboy (the 
                    // internal clock gameboy).
                }
                else
                    linkedGameboy->ioRam[0x01] = 0xff;
                if(ioRam[0x02] & 0x80) {
                    requestInterrupt(INT_SERIAL);
                    ioRam[0x02] &= ~0x80;
                }
            }
        }
        // For internal clock
        if(serialCounter > 0) {
            serialCounter -= cycles;
            if(serialCounter <= 0) {
                serialCounter = 0;
                if(linkedGameboy != NULL) {
                    linkedGameboy->cycleToSerialTransfer = cyclesSinceVBlank;
                    mgr_setInternalClockGb(this);
                    emuRet |= RET_LINK;
                    // Execution will stop here, and this gameboy's SB will be 
                    // updated when the other gameboy runs to the appropriate 
                    // cycle.
                }
                else if(printerEnabled) {
                    ioRam[0x01] = sendGbPrinterByte(ioRam[0x01]);
                }
                else
                    ioRam[0x01] = 0xff;
                requestInterrupt(INT_SERIAL);
                ioRam[0x02] &= ~0x80;
            }
            else
                setEventCycles(serialCounter);
        }

        updateTimers(cycles);

        soundCycles += cycles >> doubleSpeed;
        if(soundCycles >= soundEngine->cyclesToSoundEvent) {
            soundEngine->cyclesToSoundEvent = 10000;
            soundEngine->updateSound(soundCycles);
            soundCycles = 0;
        }
        setEventCycles(soundEngine->cyclesToSoundEvent);

        emuRet |= updateLCD(cycles);

        //interruptTriggered = ioRam[0x0F] & ioRam[0xFF];
        if(interruptTriggered) {
            /* Hack to fix Robocop 2 and LEGO Racers, possibly others. 
             * Interrupts can occur in the middle of an opcode. The result of 
             * this is that said opcode can read the resulting state - most 
             * importantly, it can read LY=144 before the vblank interrupt takes 
             * over. This is a decent approximation of that effect.
             * This has been known to break Megaman V boss intros, that's fixed 
             * by the "opTriggeredInterrupt" stuff.
             */
            if(!halt && !opTriggeredInterrupt)
                extraCycles += runOpcode(4);

            extraCycles += handleInterrupts(interruptTriggered);
            interruptTriggered = ioRam[0x0F] & ioRam[0xFF];
        }

        if(emuRet) {
            memcpy(&gbRegs, &g_gbRegs, sizeof(Registers));
            return emuRet;
        }
    }
}

void Gameboy::initGFXPalette() {
    memset(bgPaletteData, 0xff, 0x40);
    const unsigned short* palette;
    switch(gbColorize) {
        case 0:
            palette = findGbcDirPal("GBC - Grayscale");
            break;
        case 1:
            palette = gameboy->getRomFile()->gbPalette;
            break;
        case 2:
            palette = findGbcDirPal("GBC - Inverted");
            break;
        case 3:
            palette = findGbcDirPal("GBC - Pastel Mix");
            break;
        case 4:
            palette = findGbcDirPal("GBC - Red");
            break;
        case 5:
            palette = findGbcDirPal("GBC - Orange");
            break;
        case 6:
            palette = findGbcDirPal("GBC - Yellow");
            break;
        case 7:
            palette = findGbcDirPal("GBC - Green");
            break;
        case 8:
            palette = findGbcDirPal("GBC - Blue");
            break;
        case 9:
            palette = findGbcDirPal("GBC - Brown");
            break;
        case 10:
            palette = findGbcDirPal("GBC - Dark Green");
            break;
        case 11:
            palette = findGbcDirPal("GBC - Dark Blue");
            break;
        case 12:
            palette = findGbcDirPal("GBC - Dark Brown");
            break;
        default:
            palette = gameboy->getRomFile()->gbPalette;
            break;
    }

    if(gbMode == GB) {
        memcpy(bgPaletteData, palette, 4 * sizeof(u16));
        memcpy(sprPaletteData, palette + 4, 8 * sizeof(u16));
    }
}

bool Gameboy::isMainGameboy() {
    return this == gameboy;
}

void Gameboy::checkLYC() {
    if(ioRam[0x44] == ioRam[0x45]) {
        ioRam[0x41] |= 4;
        if(ioRam[0x41] & 0x40)
            requestInterrupt(INT_LCD);
    }
    else
        ioRam[0x41] &= ~4;
}

inline int Gameboy::updateLCD(int cycles) {
    if(!(ioRam[0x40] & 0x80))        // If LCD is off
    {
        scanlineCounter = 456 * (doubleSpeed ? 2 : 1);
        ioRam[0x44] = 0;
        ioRam[0x41] &= 0xF8;
        // Normally timing is synchronized with gameboy's vblank. If the screen 
        // is off, this code kicks in. The "phaseCounter" is a counter until the 
        // ds should check for input and whatnot.
        phaseCounter -= cycles;
        if(phaseCounter <= 0) {
            phaseCounter += CYCLES_PER_FRAME << doubleSpeed;
            cyclesSinceVBlank = 0;
            // Though not technically vblank, this is a good time to check for 
            // input and whatnot.
            gameboyUpdateVBlank();
            return RET_VBLANK;
        }
        return 0;
    }

    scanlineCounter -= cycles;

    if(scanlineCounter > 0) {
        setEventCycles(scanlineCounter);
        return 0;
    }

    switch(ioRam[0x41] & 3) {
        case 2: {
            ioRam[0x41]++; // Set mode 3
            scanlineCounter += 172 << doubleSpeed;
            if(isMainGameboy())
                drawScanline(ioRam[0x44]);
        }
            break;
        case 3: {
            ioRam[0x41] &= ~3; // Set mode 0

            if(ioRam[0x41] & 0x8)
                requestInterrupt(INT_LCD);

            scanlineCounter += 204 << doubleSpeed;

            if(isMainGameboy())
                drawScanline_P2(ioRam[0x44]);
            if(updateHBlankDMA()) {
                extraCycles += 8 << doubleSpeed;
            }
        }
            break;
        case 0: {
            // fall through to next case
        }
        case 1:
            if(ioRam[0x44] == 0 && (ioRam[0x41] & 3) == 1) { // End of vblank
                ioRam[0x41]++; // Set mode 2
                scanlineCounter += 80 << doubleSpeed;
            }
            else {
                ioRam[0x44]++;

                if(ioRam[0x44] < 144 || ioRam[0x44] >= 153) { // Not in vblank
                    if(ioRam[0x41] & 0x20) {
                        requestInterrupt(INT_LCD);
                    }

                    if(ioRam[0x44] >= 153) {
                        // Don't change the mode. Scanline 0 is twice as 
                        // long as normal - half of it identifies as being 
                        // in the vblank period.
                        ioRam[0x44] = 0;
                        scanlineCounter += 456 << doubleSpeed;
                    }
                    else { // End of hblank
                        ioRam[0x41] &= ~3;
                        ioRam[0x41] |= 2; // Set mode 2
                        if(ioRam[0x41] & 0x20)
                            requestInterrupt(INT_LCD);
                        scanlineCounter += 80 << doubleSpeed;
                    }
                }

                checkLYC();

                if(ioRam[0x44] >= 144) { // In vblank
                    scanlineCounter += 456 << doubleSpeed;

                    if(ioRam[0x44] == 144) // Beginning of vblank
                    {
                        ioRam[0x41] &= ~3;
                        ioRam[0x41] |= 1;   // Set mode 1

                        requestInterrupt(INT_VBLANK);
                        if(ioRam[0x41] & 0x10)
                            requestInterrupt(INT_LCD);

                        cyclesSinceVBlank = scanlineCounter - (456 << doubleSpeed);
                        gameboyUpdateVBlank();
                        setEventCycles(scanlineCounter);
                        return RET_VBLANK;
                    }
                }
            }

            break;
    }

    setEventCycles(scanlineCounter);
    return 0;
}

inline void Gameboy::updateTimers(int cycles) {
    if(ioRam[0x07] & 0x4) // Timers enabled
    {
        timerCounter -= cycles;
        while(timerCounter <= 0) {
            int clocksAdded = (-timerCounter) / timerPeriod + 1;
            timerCounter += timerPeriod * clocksAdded;
            int sum = ioRam[0x05] + clocksAdded;
            if(sum > 0xff) {
                requestInterrupt(INT_TIMER);
                ioRam[0x05] = ioRam[0x06];
            }
            else
                ioRam[0x05] = (u8) sum;
        }
        // Set cycles until the timer will trigger an interrupt.
        // Reads from [0xff05] may be inaccurate.
        // However Castlevania and Alone in the Dark are extremely slow 
        // if this is updated each time [0xff05] is changed.
        setEventCycles(timerCounter + timerPeriod * (255 - ioRam[0x05]));
    }
    dividerCounter -= cycles;
    if(dividerCounter <= 0) {
        int divsAdded = -dividerCounter / 256 + 1;
        dividerCounter += divsAdded * 256;
        ioRam[0x04] += divsAdded;
    }
    //setEventCycles(dividerCounter);
}


void Gameboy::requestInterrupt(int id) {
    ioRam[0x0F] |= id;
    interruptTriggered = (ioRam[0x0F] & ioRam[0xFF]);
    if(interruptTriggered)
        cyclesToExecute = -1;
}

void Gameboy::setDoubleSpeed(int val) {
    if(val == 0) {
        if(doubleSpeed)
            scanlineCounter >>= 1;
        doubleSpeed = 0;
        ioRam[0x4D] &= ~0x80;
    }
    else {
        if(!doubleSpeed)
            scanlineCounter <<= 1;
        doubleSpeed = 1;
        ioRam[0x4D] |= 0x80;
    }
}

void Gameboy::setRomFile(RomFile* r) {
    romFile = r;
    cheatEngine->setRomFile(r);

    // Load cheats
    if(gbsMode)
        cheatEngine->loadCheats("");
    else {
        char nameBuf[256];
        sprintf(nameBuf, "%s.cht", romFile->getBasename());
        cheatEngine->loadCheats(nameBuf);
    }
}

void Gameboy::unloadRom() {
    doAtVBlank(clearGFX);

    gameboySyncAutosave();
    if(saveFile != NULL)
        fclose(saveFile);
    saveFile = NULL;
    // unload previous save
    if(externRam != NULL) {
        free(externRam);
        externRam = NULL;
    }
    romFile = NULL;
    cheatEngine->setRomFile(NULL);
}

const char* mbcNames[] = {"ROM", "MBC1", "MBC2", "MBC3", "MBC4", "MBC5", "MBC7", "HUC1", "HUC3"};

void Gameboy::printRomInfo() {
    clearConsole();
    printf("ROM Title: \"%s\"\n", romFile->getRomTitle());
    printf("Cartridge type: %.2x (%s)\n", romFile->getMapper(), mbcNames[romFile->getMBC()]);
    printf("ROM Size: %.2x (%d banks)\n", romFile->romSlot0[0x148], romFile->getNumRomBanks());
    printf("RAM Size: %.2x (%d banks)\n", romFile->getRamSize(), numRamBanks);
}

bool Gameboy::isRomLoaded() {
    return romFile != NULL;
}

int Gameboy::loadSave(int saveId) {
    strcpy(savename, romFile->getBasename());
    if(saveId == 1)
        strcat(savename, ".sav");
    else {
        char buf[10];
        sprintf(buf, ".sa%d", saveId);
        strcat(savename, buf);
    }

    if(gbsMode)
        numRamBanks = 1;
    else {
        // Get the game's external memory size and allocate the memory
        switch(romFile->getRamSize()) {
            case 0:
                numRamBanks = 0;
                break;
            case 1:
            case 2:
                numRamBanks = 1;
                break;
            case 3:
                numRamBanks = 4;
                break;
            case 4:
                numRamBanks = 16;
                break;
            default:
                printLog("Invalid RAM bank number: %x\nDefaulting to 4 banks\n", romFile->getRamSize());
                numRamBanks = 4;
                break;
        }
        if(romFile->getMBC() == MBC2)
            numRamBanks = 1;
        else if(romFile->getMBC() == MBC7) // Probably not correct behaviour
            numRamBanks = 1;
    }

    if(numRamBanks == 0)
        return 0;

    externRam = (u8*) malloc(numRamBanks * 0x2000);

    if(gbsMode || saveId == -1)
        return 0;

    // Now load the data.
    saveFile = fopen(savename, "r+b");

    int neededFileSize = numRamBanks * 0x2000;
    if(romFile->getMBC() == MBC3 || romFile->getMBC() == HUC3)
        neededFileSize += sizeof(ClockStruct);

    int fileSize = 0;
    if(saveFile) {
        struct stat s;
        fstat(fileno(saveFile), &s);
        fileSize = s.st_size;
    }

    if(!saveFile || fileSize < neededFileSize) {
        // Extend the size of the file, or create it
        if(!saveFile) {
            saveFile = fopen(savename, "wb");
        }

        fseek(saveFile, neededFileSize - 1, SEEK_SET);
        fputc(0, saveFile);

        fclose(saveFile);
        saveFile = fopen(savename, "r+b");
    }

    fread(externRam, 1, 0x2000 * numRamBanks, saveFile);

    switch(romFile->getMBC()) {
        case MBC3:
        case HUC3:
            fread(&gbClock, 1, sizeof(gbClock), saveFile);
            break;
    }

    // Get the save file's sectors on the sd card.
    // I do this by writing a byte, then finding the area of the cache marked dirty.

#ifdef DS
    if (autoSavingEnabled) {
        flushFatCache();
        devoptab_t* devops = (devoptab_t*)GetDeviceOpTab ("sd");
        PARTITION* partition = (PARTITION*)devops->deviceData;
        CACHE* cache = partition->cache;

        memset(saveFileSectors, -1, sizeof(saveFileSectors));
        for (int i=0; i<numRamBanks*0x2000/512; i++) {
            file_seek(saveFile, i*512, SEEK_SET);
            file_putc(externRam[i*512], saveFile);
            bool found=false;
            for (int j=0; j<FAT_CACHE_SIZE; j++) {
                if (cache->cacheEntries[j].dirty) {
                    saveFileSectors[i] = cache->cacheEntries[j].sector + (i%(cache->sectorsPerPage));
                    cache->cacheEntries[j].dirty = false;
                    found = true;
                    break;
                }
            }
            if (!found) {
                printLog("couldn't find save file sector\n");
            }
        }
    }
#endif

    return 0;
}

int Gameboy::saveGame() {
    if(numRamBanks == 0 || saveFile == NULL)
        return 0;

    fseek(saveFile, 0, SEEK_SET);

    fwrite(externRam, 1, 0x2000 * numRamBanks, saveFile);

    switch(romFile->getMBC()) {
        case MBC3:
        case HUC3:
            fwrite(&gbClock, 1, sizeof(gbClock), saveFile);
            break;
    }

    flushFatCache();

    return 0;
}

void Gameboy::gameboySyncAutosave() {
    if(!autosaveStarted)
        return;

    numSaveWrites = 0;
    wroteToSramThisFrame = false;

    int numSectors = 0;
    // iterate over each 512-byte sector
    for(int i = 0; i < numRamBanks * 0x2000 / 512; i++) {
        if(dirtySectors[i]) {
            dirtySectors[i] = false;

            // If only 1 bank, it seems more efficient to write multiple sectors 
            // at once - at least, on the Acekard 2i.
            if(numRamBanks == 1) {
                writeSaveFileSectors(i / 8 * 8, 8);
                for(int j = i; j < (i / 8 * 8) + 8; j++)
                    dirtySectors[j] = false;
            }
            else {
                // For bigger saves, writing one sector at a time seems to work better.
                writeSaveFileSectors(i, 1);
                numSectors++;
            }
        }
    }
    printLog("SAVE %d sectors\n", numSectors);
    flushFatCache(); // This should do nothing, unless the RTC was written to.

    framesSinceAutosaveStarted = 0;
    autosaveStarted = false;
}

void Gameboy::updateAutosave() {
    if(autosaveStarted)
        framesSinceAutosaveStarted++;

    if(framesSinceAutosaveStarted >= 120 ||     // Executes when sram is written to for 120 consecutive frames, or
       (!saveModified && wroteToSramThisFrame)) { // when a full frame has passed since sram was last written to.
        gameboySyncAutosave();
    }
    if(saveModified && autoSavingEnabled) {
        wroteToSramThisFrame = true;
        autosaveStarted = true;
        saveModified = false;
    }
}


const int STATE_VERSION = 5;

struct StateStruct {
    // version
    // bg/sprite PaletteData
    // vram
    // wram
    // hram
    // sram
    Registers regs;
    int halt, ime;
    bool doubleSpeed, deprecated_biosOn;
    int gbMode;
    int romBank, ramBank, wramBank, vramBank;
    int memoryModel;
    ClockStruct clock;
    int scanlineCounter, timerCounter, phaseCounter, dividerCounter;
    // v2
    int serialCounter;
    // v3
    bool ramEnabled;
    // MBC-specific stuff
    // v4
    //  bool sgbMode;
    //  If sgbMode == true:
    //   int sgbPacketLength;
    //   int sgbPacketsTransferred;
    //   int sgbPacketBit;
    //   u8 sgbCommand;
    //   u8 gfxMask;
    //   u8[20*18] sgbMap;
};

void Gameboy::saveState(int stateNum) {
    if(!isRomLoaded())
        return;

    FILE* outFile;
    StateStruct state;
    char statename[100];

    if(stateNum == -1)
        sprintf(statename, "%s.yss", romFile->getBasename());
    else
        sprintf(statename, "%s.ys%d", romFile->getBasename(), stateNum);
    outFile = fopen(statename, "w");

    if(outFile == 0) {
        printMenuMessage("Error opening file for writing.");
        return;
    }

    state.regs = gbRegs;
    state.halt = halt;
    state.ime = ime;
    state.doubleSpeed = doubleSpeed;
    state.gbMode = gbMode;
    state.romBank = romBank;
    state.ramBank = currentRamBank;
    state.wramBank = wramBank;
    state.vramBank = vramBank;
    state.memoryModel = memoryModel;
    state.clock = gbClock;
    state.scanlineCounter = scanlineCounter;
    state.timerCounter = timerCounter;
    state.phaseCounter = phaseCounter;
    state.dividerCounter = dividerCounter;
    state.serialCounter = serialCounter;
    state.ramEnabled = ramEnabled;

    fwrite(&STATE_VERSION, sizeof(int), 1, outFile);
    fwrite((char*) bgPaletteData, 1, sizeof(bgPaletteData), outFile);
    fwrite((char*) sprPaletteData, 1, sizeof(sprPaletteData), outFile);
    fwrite((char*) vram, 1, sizeof(vram), outFile);
    fwrite((char*) wram, 1, sizeof(wram), outFile);
    fwrite((char*) hram, 1, 0x200, outFile);
    fwrite((char*) externRam, 1, 0x2000 * numRamBanks, outFile);

    fwrite((char*) &state, 1, sizeof(StateStruct), outFile);

    switch(romFile->getMBC()) {
        case HUC3:
            fwrite(&HuC3Mode, 1, sizeof(u8), outFile);
            fwrite(&HuC3Value, 1, sizeof(u8), outFile);
            fwrite(&HuC3Shift, 1, sizeof(u8), outFile);
            break;
    }

    fwrite(&sgbMode, 1, sizeof(bool), outFile);
    if(sgbMode) {
        fwrite(&sgbPacketLength, 1, sizeof(int), outFile);
        fwrite(&sgbPacketsTransferred, 1, sizeof(int), outFile);
        fwrite(&sgbPacketBit, 1, sizeof(int), outFile);
        fwrite(&sgbCommand, 1, sizeof(u8), outFile);
        fwrite(&gfxMask, 1, sizeof(u8), outFile);
        fwrite(sgbMap, 1, sizeof(sgbMap), outFile);
    }

    fclose(outFile);
}

int Gameboy::loadState(int stateNum) {
    if(!isRomLoaded())
        return 1;

    FILE* inFile;
    StateStruct state;
    char statename[256];
    int version;

    memset(&state, 0, sizeof(StateStruct));

    if(stateNum == -1)
        sprintf(statename, "%s.yss", romFile->getBasename());
    else
        sprintf(statename, "%s.ys%d", romFile->getBasename(), stateNum);
    inFile = fopen(statename, "r");

    if(inFile == 0) {
        printMenuMessage("State doesn't exist.");
        return 1;
    }

    fread(&version, sizeof(int), 1, inFile);

    if(version == 0 || version > STATE_VERSION) {
        printMenuMessage("State is incompatible.");
        return 1;
    }

    fread((char*) bgPaletteData, 1, sizeof(bgPaletteData), inFile);
    fread((char*) sprPaletteData, 1, sizeof(sprPaletteData), inFile);
    fread((char*) vram, 1, sizeof(vram), inFile);
    fread((char*) wram, 1, sizeof(wram), inFile);
    fread((char*) hram, 1, 0x200, inFile);

    if(version <= 4 && romFile->getRamSize() == 0x04)
        // Value "0x04" for ram size wasn't interpreted correctly before
        fread((char*) externRam, 1, 0x2000 * 4, inFile);
    else
        fread((char*) externRam, 1, 0x2000 * numRamBanks, inFile);

    fread((char*) &state, 1, sizeof(StateStruct), inFile);

    /* MBC-specific values have been introduced in v3 */
    if(version >= 3) {
        switch(romFile->getMBC()) {
            case MBC3:
                if(version == 3) {
                    u8 rtcReg;
                    fread(&rtcReg, 1, sizeof(u8), inFile);
                    if(rtcReg != 0)
                        currentRamBank = rtcReg;
                }
                break;
            case HUC3:
                fread(&HuC3Mode, 1, sizeof(u8), inFile);
                fread(&HuC3Value, 1, sizeof(u8), inFile);
                fread(&HuC3Shift, 1, sizeof(u8), inFile);
                break;
        }

        fread(&sgbMode, 1, sizeof(bool), inFile);
        if(sgbMode) {
            fread(&sgbPacketLength, 1, sizeof(int), inFile);
            fread(&sgbPacketsTransferred, 1, sizeof(int), inFile);
            fread(&sgbPacketBit, 1, sizeof(int), inFile);
            fread(&sgbCommand, 1, sizeof(u8), inFile);
            fread(&gfxMask, 1, sizeof(u8), inFile);
            fread(sgbMap, 1, sizeof(sgbMap), inFile);
        }
    }
    else
        sgbMode = false;


    fclose(inFile);
    if(stateNum == -1) {
        fsDelete(statename);
    }

    gbRegs = state.regs;
    halt = state.halt;
    ime = state.ime;
    doubleSpeed = state.doubleSpeed;
    gbMode = state.gbMode;
    romBank = state.romBank;
    currentRamBank = state.ramBank;
    wramBank = state.wramBank;
    vramBank = state.vramBank;
    memoryModel = state.memoryModel;
    gbClock = state.clock;
    scanlineCounter = state.scanlineCounter;
    timerCounter = state.timerCounter;
    phaseCounter = state.phaseCounter;
    dividerCounter = state.dividerCounter;
    serialCounter = state.serialCounter;
    ramEnabled = state.ramEnabled;
    if(version < 3)
        ramEnabled = true;

    timerPeriod = periods[ioRam[0x07] & 0x3];
    cyclesToEvent = 1;

    mapMemory();
    setDoubleSpeed(doubleSpeed);


    if(autoSavingEnabled && stateNum != -1)
        saveGame(); // Synchronize save file on sd with file in ram

    refreshGFX();
    soundEngine->refresh();

    return 0;
}

void Gameboy::deleteState(int stateNum) {
    if(!isRomLoaded())
        return;

    if(!checkStateExists(stateNum))
        return;

    char statename[256];

    if(stateNum == -1)
        sprintf(statename, "%s.yss", romFile->getBasename());
    else
        sprintf(statename, "%s.ys%d", romFile->getBasename(), stateNum);
    fsDelete(statename);
}

bool Gameboy::checkStateExists(int stateNum) {
    if(!isRomLoaded())
        return false;

    char statename[256];

    if(stateNum == -1)
        sprintf(statename, "%s.yss", romFile->getBasename());
    else
        sprintf(statename, "%s.ys%d", romFile->getBasename(), stateNum);
    return fsExists(statename);
    /*
    file = fopen(statename, "r");

    if (file == 0) {
        return false;
    }
    fclose(file);
    return true;
    */
}
