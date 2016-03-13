#pragma once

#include <stdio.h>

#include "types.h"

class MMU {
public:
    MMU(Gameboy* gameboy);

    void reset();

    void loadState(FILE* file, int version);
    void saveState(FILE* file);

    u8 read(u16 addr);
    void write(u16 addr, u8 val);

    void mapBlock(u8 bank, u8* block);
    void mapReadFunc(u8 bank, void* data, u8 (*readFunc)(void* data, u16 addr));
    void mapWriteFunc(u8 bank, void* data, void (*writeFunc)(void* data, u16 addr, u8 val));
    void unmap(u8 bank);

    inline u8 getWramBank() {
        return this->wramBank;
    }

    inline void setWramBank(u8 bank) {
        this->wramBank = bank;
    }
private:
    u8 readRomBank0(u16 addr);
    u8 readBankF(u16 addr);
    void writeBankF(u16 addr, u8 val);

    static u8 readRomBank0Entry(void* data, u16 addr);
    static u8 readBankFEntry(void* data, u16 addr);
    static void writeBankFEntry(void* data, u16 addr, u8 val);

    void mapBanks();

    Gameboy* gameboy;

    u8* mappedBlocks[0x10];
    u8 (*mappedReadFuncs[0x10])(void* data, u16 addr);
    void* mappedReadFuncData[0x10];
    void (*mappedWriteFuncs[0x10])(void* data, u16 addr, u8 val);
    void* mappedWriteFuncData[0x10];

    u8 wramBank;

    u8 wram[8][0x1000];
    u8 hram[0x7F];
};