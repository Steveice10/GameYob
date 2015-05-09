#pragma once

#include <stdio.h>
#include <vector>
#include <stdarg.h>

#include "gb_apu/Multi_Buffer.h"
#include "gb_apu/Gb_Apu.h"
#include "types.h"
#include "cheatengine.h"
#include "ppu.h"
#include "printer.h"
#include "romfile.h"

#define MAX_SRAM_SIZE   0x20000

// IMPORTANT: This is unchanging, it DOES NOT change in double speed mode!
#define clockSpeed 4194304

// Same deal
#define CYCLES_PER_FRAME 70224
#define CYCLES_UNTIL_SAMPLE 84

#define FRAMES_PER_BUFFER 8
#define SAMPLE_RATE (CYCLES_PER_FRAME * 59.7 / CYCLES_UNTIL_SAMPLE)
#define CYCLES_PER_BUFFER (CYCLES_PER_FRAME * FRAMES_PER_BUFFER)
#define APU_BUFFER_SIZE (CYCLES_PER_BUFFER / CYCLES_UNTIL_SAMPLE)

#define GB            0
#define CGB            1

// Interrupts
#define INT_VBLANK  0x01
#define INT_LCD     0x02
#define INT_TIMER   0x04
#define INT_SERIAL  0x08
#define INT_JOYPAD  0x10

// Return codes for runEmul
#define RET_VBLANK  1
#define RET_LINK    2

// Be careful changing this; it affects save state compatibility.
struct ClockStruct {
    union {
        struct {
            int s, m, h, d, ctrl;
            int u[1]; /* Unused */
        } mbc3;
        struct {
            int m, d, y;
            int u[3]; /* Unused */
        } huc3;
        struct {
            int s, m, h, d, mon, y;
        } tama5;
    };

    /* Unused */
    int u[4];
    /* For VBA/Lameboy compatibility */
    time_t last;
};

// Cpu stuff
typedef union {
    u16 w;
    struct B {
        u8 l;
        u8 h;
    } b;
} Register;

struct Registers {
    Register sp;
    /* Stack Pointer */
    Register pc;
    /* Program Counter */
    Register af;
    Register bc;
    Register de;
    Register hl;
};

class Gameboy {
public:
    // gameboy.cpp

    Gameboy();
    ~Gameboy();
    void init();
    void initGBMode();
    void initGBCMode();
    void initSND();
    void initGFXPalette();

    inline void setEventCycles(int cycles) {
        if(cycles < cyclesToEvent) {
            cyclesToEvent = cycles > 0 ? cycles : 0;
        }
    }


    void gameboyCheckInput();
    void gameboyUpdateVBlank();

    void resetGameboy(); // This may be called even from the context of "runOpcode".
    void pause();
    void unpause();
    bool isGameboyPaused();
    int runEmul();
    void initGameboyMode();
    void checkLYC();
    int updateLCD(int cycles);
    void updateTimers(int cycles);
    void updateSound(int cycles);
    void updateSerial(int cycles);
    void requestInterrupt(int id);
    void setDoubleSpeed(int val);

    void loadBorder();
    void setRomFile(const char* filename);
    void unloadRom();
    bool isRomLoaded();

    int loadSave(int saveId);
    int saveGame();
    void gameboySyncAutosave();
    void updateAutosave();

    void saveState(int num);
    int loadState(int num);
    void deleteState(int num);
    bool checkStateExists(int num);

    inline GameboyPrinter* getPrinter() { return printer; }

    inline GameboyPPU* getPPU() { return ppu; }

    inline Gb_Apu* getAPU() { return apu; }

    inline CheatEngine* getCheatEngine() { return cheatEngine; }

    inline RomFile* getRomFile() { return romFile; }

    // variables
    Gameboy* linkedGameboy;

    u8 controllers[4];

    bool doubleSpeed;

    u8 bios[0x900];
    bool biosLoaded;
    bool biosOn;

    int gbMode;
    bool sgbMode;

    int scanlineCounter;
    int phaseCounter;
    int dividerCounter;
    int timerCounter;
    int serialCounter;
    int timerPeriod;
    long periods[4];

    int cyclesToEvent;
    int cyclesSinceVBlank;
    int interruptTriggered;
    int gameboyFrameCounter;

    int emuRet;
    int cycleToSerialTransfer;

    int halt;
    bool haltBug;
    int ime;
    int extraCycles;
    int soundCycles;
    int cyclesToExecute;
    struct Registers gbRegs;

private:
    volatile bool gameboyPaused;
    bool resettingGameboy;

    GameboyPrinter* printer;

    GameboyPPU* ppu;

    Gb_Apu* apu;
    Mono_Buffer* leftBuffer;
    Mono_Buffer* rightBuffer;
    Mono_Buffer* centerBuffer;

    CheatEngine* cheatEngine;
    RomFile* romFile;

    FILE* saveFile;
    char savename[256];

    // gbcpu.cpp

public:
    void initCPU();
    void enableInterrupts();
    void disableInterrupts();
    int handleInterrupts(unsigned int interruptTriggered);
    int runOpcode(int cycles);

    inline u8 quickRead(u16 addr) { return memory[addr >> 12][addr & 0xFFF]; }

    inline u8 quickReadIO(u8 addr) { return ioRam[addr]; }

    inline u16 quickRead16(u16 addr) { return quickRead(addr) | (quickRead(addr + 1) << 8); }

    // Currently unused because this can actually overwrite the rom, in rare cases
    inline void quickWrite(u16 addr, u8 val) { memory[addr >> 12][addr & 0xFFF] = val; }

private:
    struct Registers g_gbRegs;
    u8* firstPcAddr;

    // mmu.cpp

public:
    void initMMU();
    void mapMemory();

    u8 readMemoryFast(u16 addr);
    u16 readMemory16(u16 addr);
    u8 readIO(u8 ioReg);

    void writeIO(u8 ioReg, u8 val);
    void refreshP1();
    u8 readMemoryOther(u16 addr);
    void writeMemoryOther(u16 addr, u8 val);

    inline u8 readMemory(u16 addr) {
        int area = addr >> 12;
        if(!(area & 0x8) || area == 0xc || area == 0xd) {
            return memory[area][addr & 0xfff];
        }
        else
            return readMemoryOther(addr);
    }

    inline void writeMemory(u16 addr, u8 val) {
        int area = addr >> 12;
        if(area == 0xc) {
            // Checking for this first is a tiny bit more efficient.
            wram[0][addr & 0xfff] = val;
            return;
        }
        else if(area == 0xd) {
            wram[wramBank][addr & 0xfff] = val;
            return;
        }
        writeMemoryOther(addr, val);
    }


    bool updateHBlankDMA();
    void latchClock();

    inline u8 getWramBank() { return wramBank; }

    inline void setWramBank(u8 bank) { wramBank = bank; }

    void refreshRomBank0(int bank);
    void refreshRomBank1(int bank);
    void refreshRamBank(int bank);
    void writeSram(u16 addr, u8 val);

    // mmu variables

    int resultantGBMode;

    // memory[x][yyy] = ram value at xyyy
    u8* memory[0x10];

    u8 vram[2][0x2000];
    u8* externRam;
    u8 wram[8][0x1000];

    u8 highram[0x1000];
    u8* const hram;
    u8* const ioRam;

    u8 bgPaletteData[0x40];
    u8 sprPaletteData[0x40];

    int wramBank;
    int vramBank;

    u16 dmaSource;
    u16 dmaDest;
    u16 dmaLength;
    int dmaMode;

    bool saveModified;
    bool dirtySectors[MAX_SRAM_SIZE / 512];
    bool autosaveStarted;

    int rumbleValue;
    int lastRumbleValue;

    bool wroteToSramThisFrame;
    int framesSinceAutosaveStarted;

    void (Gameboy::*writeFunc)(u16, u8);
    u8 (Gameboy::*readFunc)(u16);

    bool suspendStateExists;

    int saveFileSectors[MAX_SRAM_SIZE / 512];


    // mbc.cpp

    u8 m3r(u16 addr);
    u8 m7r(u16 addr);
    u8 h3r(u16 addr);
    u8 camr(u16 addr);

    void m0w(u16 addr, u8 val);
    void m1w(u16 addr, u8 val);
    void m2w(u16 addr, u8 val);
    void m3w(u16 addr, u8 val);
    void m5w(u16 addr, u8 val);
    void m7w(u16 addr, u8 val);
    void mmm01w(u16 addr, u8 val);
    void h1w(u16 addr, u8 val);
    void h3w(u16 addr, u8 val);
    void camw(u16 addr, u8 val);
    void t5w(u16 addr, u8 val);

    void writeClockStruct();


    // mbc variables

    ClockStruct gbClock;

    bool ramEnabled;

    int memoryModel;
    bool hasClock;
    int romBank0Num;
    int romBank1Num;
    int ramBankNum;

    bool rockmanMapper;

    // HuC3
    u8 HuC3Mode;
    u8 HuC3Value;
    u8 HuC3Shift;

    // MBC7
    bool mbc7WriteEnable;
    bool mbc7Idle;
    u8 mbc7Cs;
    u8 mbc7Sk;
    u8 mbc7OpCode;
    u8 mbc7Addr;
    u8 mbc7Count;
    u8 mbc7State;
    u16 mbc7Buffer;
    u8 mbc7RA; // Ram Access register 0xa080

    // MMM01
    bool mmm01BankSelected;
    u8 mmm01RomBaseBank;

    // CAMERA
    bool cameraIO;

    // TAMA5
    int tama5CommandNumber;
    int tama5RamByteSelect;
    u8 tama5Commands[0x10];
    u8 tama5RAM[0x100];

    // sgb.cpp
public:
    void sgbInit();
    void sgbHandleP1(u8 val);
    u8 sgbReadP1();

    u8 sgbMap[20 * 18];

    void setBackdrop(u16 val);
    void sgbLoadAttrFile(int index);
    void sgbDoVramTransfer(u8* dest);

    // Begin commands
    void sgbPalXX(int block);
    void sgbAttrBlock(int block);
    void sgbAttrLin(int block);
    void sgbAttrDiv(int block);
    void sgbAttrChr(int block);
    void sgbSound(int block);
    void sgbSouTrn(int block);
    void sgbPalSet(int block);
    void sgbPalTrn(int block);
    void sgbAtrcEn(int block);
    void sgbTestEn(int block);
    void sgbIconEn(int block);
    void sgbDataSnd(int block);
    void sgbDataTrn(int block);
    void sgbMltReq(int block);
    void sgbJump(int block);
    void sgbChrTrn(int block);
    void sgbPctTrn(int block);
    void sgbAttrTrn(int block);
    void sgbAttrSet(int block);
    void sgbMaskEn(int block);
    void sgbObjTrn(int block);
    // End commands

private:

    int sgbPacketLength; // Number of packets to be transferred this command
    int sgbPacketsTransferred; // Number of packets which have been transferred so far
    int sgbPacketBit; // Next bit # to be sent in the packet. -1 if no packet is being transferred.
    u8 sgbPacket[16];
    u8 sgbCommand;

    u8 sgbNumControllers;
    u8 sgbSelectedController; // Which controller is being observed
    u8 sgbButtonsChecked;

    // Data for various different sgb commands
    struct SgbCmdData {
        int numDataSets;

        union {
            struct {
                u8 data[6];
                u8 dataBytes;
            } attrBlock;

            struct {
                u8 writeStyle;
                u8 x, y;
            } attrChr;
        };
    } sgbCmdData;
};

extern Gameboy* gameboy;