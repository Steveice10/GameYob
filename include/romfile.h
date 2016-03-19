#pragma once

#include <stdio.h>

#include "types.h"

typedef enum : u8 {
    MBC0 = 0,
    MBC1,
    MBC2,
    MBC3,
    MBC5,
    MBC7,
    MMM01,
    HUC1,
    HUC3,
    CAMERA,
    TAMA5,

    MBC_MAX
} MBCType;

class Gameboy;

class GBS;

class RomFile {
public:
    RomFile(u8* rom, u32 size);
    ~RomFile();

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

    inline bool hasRumble() {
        return this->rumble;
    }

    inline u8* getRomBank(int bank) {
        if(bank < 0 || bank >= this->totalRomBanks) {
            return NULL;
        }

        return &this->rom[bank * 0x4000];
    }
private:
    u8* rom = NULL;

    std::string romTitle = "";
    int totalRomBanks = 0;
    int totalRamBanks = 0;
    MBCType mbcType = MBC0;
    bool rumble = false;
};