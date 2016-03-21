#pragma once

#include <istream>
#include <ostream>

#include "types.h"

class MBC {
public:
    MBC(Gameboy* gameboy);
    ~MBC();

    void reset();

    void loadState(std::istream& data, u8 version);
    void saveState(std::ostream &data);

    void update();

    void load(std::istream& data);
    void save(std::ostream& data);
private:
    u8 readSram(u16 addr);
    void writeSram(u16 addr, u8 val);

    void mapRomBank0();
    void mapRomBank1();
    void mapSramBank();
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
    void m6w(u16 addr, u8 val);
    void m7w(u16 addr, u8 val);
    void mmm01w(u16 addr, u8 val);
    void h1w(u16 addr, u8 val);
    void h3w(u16 addr, u8 val);
    void camw(u16 addr, u8 val);
    void t5w(u16 addr, u8 val);

    void camu();

    void camTakePicture();

    void latchClock();

    typedef void (MBC::*mbcWrite)(u16, u8);
    typedef u8 (MBC::*mbcRead)(u16);
    typedef void (MBC::*mbcUpdate)();

    const mbcRead mbcReads[12] = {
            NULL,
            NULL,
            NULL,
            &MBC::m3r,
            NULL,
            NULL,
            &MBC::m7r,
            NULL,
            NULL,
            &MBC::h3r,
            &MBC::camr,
            NULL
    };

    const mbcWrite mbcWrites[12] = {
            &MBC::m0w,
            &MBC::m1w,
            &MBC::m2w,
            &MBC::m3w,
            &MBC::m5w,
            &MBC::m6w,
            &MBC::m7w,
            &MBC::mmm01w,
            &MBC::h1w,
            &MBC::h3w,
            &MBC::camw,
            &MBC::t5w
    };

    const mbcUpdate mbcUpdates[12] = {
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            &MBC::camu,
            NULL
    };

    Gameboy* gameboy = NULL;

    mbcRead readFunc;
    mbcWrite writeFunc;
    mbcUpdate updateFunc;

    u8* sram;

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

    // General
    s32 romBank0;
    s32 romBank1;
    s32 sramBank;
    bool sramEnabled;

    // MBC1
    bool mbc1RockmanMapper;
    bool mbc1RamMode;

    // MBC6
    s32 romBank1ALatch;
    s32 romBank1BLatch;
    s32 romBank1A;
    s32 romBank1B;
    s32 romBank1C;
    s32 romBank1D;

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

    // HuC1
    bool huc1RamMode;

    // HuC3
    u8 huc3Mode;
    u8 huc3Value;
    u8 huc3Shift;

    // MMM01
    bool mmm01BankSelected;
    u8 mmm01RomBaseBank;

    // CAMERA
    bool cameraIO;
    u64 cameraReadyCycle;
    u8 cameraRegs[0x36];

    // TAMA5
    s32 tama5CommandNumber;
    s32 tama5RamByteSelect;
    u8 tama5Commands[0x10];
    u8 tama5RAM[0x100];
};