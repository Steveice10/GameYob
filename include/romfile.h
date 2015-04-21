#pragma once

#include <stdio.h>
#include <string>

#include <ctrcommon/types.hpp>

/* All the possible MBC */
typedef enum {
    MBC0 = 0,
    MBC1,
    MBC2,
    MBC3,
    MBC5,
    MBC7,
    MMM01,
    HUC1,
    HUC3
} MBC;

class Gameboy;

class RomFile {
public:
    RomFile(Gameboy* gb, const std::string path);
    ~RomFile();

    u8* getRomBank(int bank);
    void printInfo();

    inline bool isLoaded() {
        return this->loaded;
    }

    inline std::string getFileName() {
        return this->fileName;
    }

    inline const std::string getRomTitle() {
        return this->romTitle;
    }

    inline bool isCgbSupported() {
        return this->cgbSupported;
    }

    inline bool isCgbRequired() {
        return this->cgbRequired;
    }

    inline int getRomBanks() {
        return this->totalRomBanks;
    }

    inline int getRamBanks() {
        return this->totalRamBanks;
    }

    inline bool isSgbEnhanced() {
        return this->sgb;
    }

    inline MBC getMBC() {
        return this->mbc;
    }

    inline bool hasRumble() {
        return this->rumble;
    }
private:
    Gameboy* gameboy = NULL;
    FILE* file = NULL;

    bool loaded = true;

    u8** banks = NULL;
    bool firstBanksAtEnd = false;

    std::string fileName = "";
    std::string romTitle = "";
    bool cgbSupported = false;
    bool cgbRequired = false;
    u8 rawRomSize = 0;
    int totalRomBanks = 0;
    u8 rawRamSize = 0;
    int totalRamBanks = 0;
    bool sgb = false;
    u8 rawMBC = 0;
    MBC mbc = MBC0;
    bool rumble = false;
};
