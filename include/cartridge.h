#pragma once

#include <istream>
#include <ostream>

#include "types.h"

class Gameboy;

typedef enum : u8 {
    MBC0 = 0,
    MBC1,
    MBC2,
    MBC3,
    MBC5,
    MBC6,
    MBC7,
    MMM01,
    HUC1,
    HUC3,
    CAMERA,
    TAMA5,

    MBC_MAX
} MBCType;

class Cartridge {
public:
    Cartridge(std::istream& romData, int romSize, std::istream& saveData, int saveSize);
    ~Cartridge();

    void reset(Gameboy* gameboy);

    void loadState(std::istream& data, u8 version);
    void saveState(std::ostream &data);

    void update();

    void save(std::ostream& data);

    inline const std::string getRomTitle() {
        return this->romTitle;
    }

    inline bool isCgbSupported() {
        return this->rom[0x0143] == 0x80 || this->isCgbRequired();
    }

    inline bool isCgbRequired() {
        return this->rom[0x0143] == 0xC0;
    }

    inline u8 getRawRomSize() {
        return this->rom[0x0148];
    }

    inline int getRomBanks() {
        return this->totalRomBanks;
    }

    inline u8 getRawRamSize() {
        return this->rom[0x0149];
    }

    inline int getRamBanks() {
        return this->totalRamBanks;
    }

    inline bool isSgbEnhanced() {
        return this->rom[0x146] == 0x03 && this->rom[0x014B] == 0x33;
    }

    inline u8 getRawMBC() {
        return this->rom[0x0147];
    }

    inline MBCType getMBCType() {
        return this->mbcType;
    }

    inline u8* getRomBank(int bank) {
        if(bank < 0 || bank >= this->totalRomBanks) {
            return NULL;
        }

        return &this->rom[bank * 0x4000];
    }
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

    typedef void (Cartridge::*mbcWrite)(u16, u8);
    typedef u8 (Cartridge::*mbcRead)(u16);
    typedef void (Cartridge::*mbcUpdate)();

    const mbcRead mbcReads[MBC_MAX] = {
            NULL,
            NULL,
            NULL,
            &Cartridge::m3r,
            NULL,
            NULL,
            &Cartridge::m7r,
            NULL,
            NULL,
            &Cartridge::h3r,
            &Cartridge::camr,
            NULL
    };

    const mbcWrite mbcWrites[MBC_MAX] = {
            &Cartridge::m0w,
            &Cartridge::m1w,
            &Cartridge::m2w,
            &Cartridge::m3w,
            &Cartridge::m5w,
            &Cartridge::m6w,
            &Cartridge::m7w,
            &Cartridge::mmm01w,
            &Cartridge::h1w,
            &Cartridge::h3w,
            &Cartridge::camw,
            &Cartridge::t5w
    };

    const mbcUpdate mbcUpdates[MBC_MAX] = {
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
            &Cartridge::camu,
            NULL
    };

    Gameboy* gameboy;

    u8* rom;
    u8* sram;

    std::string romTitle;
    int totalRomBanks;
    int totalRamBanks;
    MBCType mbcType;
    bool rockmanMapper;
    bool rumble;

    mbcRead readFunc;
    mbcWrite writeFunc;
    mbcUpdate updateFunc;

    struct {
        u32 seconds;
        u32 minutes;
        u32 hours;
        u32 days;
        u32 months;
        u32 years;

        u64 last;
    } gbClock;

    // General
    s32 romBank0;
    s32 romBank1;
    s32 sramBank;
    bool sramEnabled;

    // MBC1
    bool mbc1RamMode;

    // MBC3
    u8 mbc3Ctrl;

    // MBC6
    s32 romBank1ALatch;
    s32 romBank1BLatch;
    s32 romBank1A;
    s32 romBank1B;

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