#pragma once

#include <stdio.h>

#include "romfile.h"
#include "types.h"

class MBC {
public:
    MBC(Gameboy* gameboy);
    ~MBC();

    void reset();
    void save();

    void loadState(FILE* file, int version);
    void saveState(FILE* file);
private:
    u8* getRamBank(int bank);

    u8 readSram(u16 addr);
    void writeSram(u16 addr, u8 val);

    void mapRomBank0(int bank);
    void mapRomBank1(int bank);
    void mapRamBank(int bank);
    void mapBanks();

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

    void latchClock();

    typedef void (MBC::*mbcWrite)(u16, u8);
    typedef u8 (MBC::*mbcRead)(u16);

    const mbcRead mbcReads[MBC_COUNT] = {
            NULL,
            NULL,
            NULL,
            &MBC::m3r,
            NULL,
            &MBC::m7r,
            NULL,
            NULL,
            &MBC::h3r,
            &MBC::camr,
            NULL
    };

    const mbcWrite mbcWrites[MBC_COUNT] = {
            &MBC::m0w,
            &MBC::m1w,
            &MBC::m2w,
            &MBC::m3w,
            &MBC::m5w,
            &MBC::m7w,
            &MBC::mmm01w,
            &MBC::h1w,
            &MBC::h3w,
            &MBC::camw,
            &MBC::t5w
    };

    Gameboy* gameboy = NULL;

    FILE* file;

    mbcRead readFunc;
    mbcWrite writeFunc;

    int ramBanks;
    u8** banks;
    bool dirtyBanks[16];

    struct {
        union {
            struct {
                s32 s, m, h, d, ctrl;
                s32 u[1]; /* Unused */
            } mbc3;
            struct {
                s32 m, d, y;
                s32 u[3]; /* Unused */
            } huc3;
            struct {
                s32 s, m, h, d, mon, y;
            } tama5;
        };

        /* Unused */
        s32 u[4];

        u32 last;
    } gbClock;
    bool dirtyClock;

    // Type
    MBCType mbcType;
    bool rockmanMapper;

    // General
    s32 romBank0Num;
    s32 romBank1Num;
    s32 ramBankNum;
    bool ramEnabled;
    s32 memoryModel;

    // HuC3
    u8 HuC3Mode;
    u8 HuC3Value;
    u8 HuC3Shift;

    // MBC7
    bool mbc7WriteEnable;
    u8 mbc7Cs;
    u8 mbc7Sk;
    u8 mbc7OpCode;
    u8 mbc7Addr;
    u8 mbc7Count;
    u8 mbc7State;
    u16 mbc7Buffer;
    u8 mbc7RA;

    // MMM01
    bool mmm01BankSelected;
    u8 mmm01RomBaseBank;

    // CAMERA
    bool cameraIO;

    // TAMA5
    s32 tama5CommandNumber;
    s32 tama5RamByteSelect;
    u8 tama5Commands[0x10];
    u8 tama5RAM[0x100];
};