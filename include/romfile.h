#pragma once

#include <stdio.h>
#include <vector>

#include <ctrcommon/types.hpp>

#define MAX_ROM_BANKS   0x200

/* All the possible MBC */
enum {
    MBC0 = 0,
    MBC1,
    MBC2,
    MBC3,
    MBC5,
    MBC7,
    HUC1,
    HUC3,
    MBC_MAX,
};

class Gameboy;

class RomFile {
public:
    RomFile(Gameboy* gb, const char* filename);
    ~RomFile();

    void loadRomBank(int romBank);
    bool isRomBankLoaded(int bank);
    u8* getRomBank(int bank);
    const char* getBasename();

    char* getRomTitle();

    inline int getNumRomBanks() { return numRomBanks; }

    inline int getCgbFlag() { return romSlot0[0x143]; }

    inline int getRamSize() { return romSlot0[0x149]; }

    inline int getMapper() { return romSlot0[0x147]; }

    inline int getMBC() { return MBC; }

    inline bool hasRumble() { return getMapper() == 0x1c || getMapper() == 0x1d || getMapper() == 0x1e; }

    u8* romSlot0;
    u8* romSlot1;

private:
    Gameboy* gameboy;

    u8* romBankSlots = NULL; // Each 0x4000 bytes = one slot

    int numRomBanks;
    int maxLoadedRomBanks;
    int numLoadedRomBanks;
    int bankSlotIDs[MAX_ROM_BANKS]; // Keeps track of which bank occupies which slot
    std::vector<int> lastBanksUsed;

    FILE* romFile;
    char filename[256];
    char basename[256];
    char romTitle[20];

    int MBC;
};
