#pragma once

#include <stdio.h>

#include "types.h"

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
    HUC3,
    CAMERA,
    TAMA5
} MBC;

class Gameboy;

class GBS;

class RomFile {
public:
    RomFile(Gameboy* gb, const std::string path);
    ~RomFile();

    u8* getRomBank(int bank);

    inline bool isLoaded() {
        return this->loaded;
    }

    inline std::string getFileName() {
        return this->fileName;
    }

    inline bool isGBS() {
        return this->gbs;
    }

    inline GBS* getGBS() {
        return this->gbsInfo;
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

    inline u8 getRawRomSize() {
        return this->rawRomSize;
    }

    inline int getRomBanks() {
        return this->totalRomBanks;
    }

    inline u8 getRawRamSize() {
        return this->rawRamSize;
    }

    inline int getRamBanks() {
        return this->totalRamBanks;
    }

    inline bool isSgbEnhanced() {
        return this->sgb;
    }

    inline u8 getRawMBC() {
        return this->rawMBC;
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

    bool gbs = false;
    GBS* gbsInfo = NULL;

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

class GBS {
public:
    GBS(RomFile* romFile, u8* header);

    void init(Gameboy* gameboy);
    void playSong(Gameboy* gameboy, int song);
    void stopSong(Gameboy* gameboy);

    inline u8 getSongCount() {
        return this->songCount;
    }

    inline u8 getFirstSong() {
        return this->firstSong;
    }

    inline u16 getLoadAddress() {
        return this->loadAddress;
    }

    inline u16 getInitAddress() {
        return this->initAddress;
    }

    inline u16 getPlayAddress() {
        return this->playAddress;
    }

    inline u16 getStackPointer() {
        return this->stackPointer;
    }

    inline u8 getTimerModulo() {
        return this->timerModulo;
    }

    inline u8 getTimerControl() {
        return this->timerControl;
    }

    inline std::string getTitle() {
        return this->title;
    }

    inline std::string getAuthor() {
        return this->author;
    }

    inline std::string getCopyright() {
        return this->copyright;
    }
private:
    RomFile* rom = NULL;

    u8 songCount = 0;
    u8 firstSong = 0;
    u16 loadAddress = 0;
    u16 initAddress = 0;
    u16 playAddress = 0;
    u16 stackPointer = 0;
    u8 timerModulo = 0;
    u8 timerControl = 0;
    std::string title = "";
    std::string author = "";
    std::string copyright = "";
};
