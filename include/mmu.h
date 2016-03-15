#pragma once

#include <stdio.h>

#include <functional>

#include "types.h"

#define JOYP 0xFF00
#define SB 0xFF01
#define SC 0xFF02
#define DIV 0xFF04
#define TIMA 0xFF05
#define TMA 0xFF06
#define TAC 0xFF07
#define IF 0xFF0F
#define NR10 0xFF10
#define NR11 0xFF11
#define NR12 0xFF12
#define NR13 0xFF13
#define NR14 0xFF14
#define NR20 0xFF15
#define NR21 0xFF16
#define NR22 0xFF17
#define NR23 0xFF18
#define NR24 0xFF19
#define NR30 0xFF1A
#define NR31 0xFF1B
#define NR32 0xFF1C
#define NR33 0xFF1D
#define NR34 0xFF1E
#define NR40 0xFF1F
#define NR41 0xFF20
#define NR42 0xFF21
#define NR43 0xFF22
#define NR44 0xFF23
#define NR50 0xFF24
#define NR51 0xFF25
#define NR52 0xFF26
#define WAVE0 0xFF30
#define WAVE1 0xFF31
#define WAVE2 0xFF32
#define WAVE3 0xFF33
#define WAVE4 0xFF34
#define WAVE5 0xFF35
#define WAVE6 0xFF36
#define WAVE7 0xFF37
#define WAVE8 0xFF38
#define WAVE9 0xFF39
#define WAVEA 0xFF3A
#define WAVEB 0xFF3B
#define WAVEC 0xFF3C
#define WAVED 0xFF3D
#define WAVEE 0xFF3E
#define WAVEF 0xFF3F
#define LCDC 0xFF40
#define STAT 0xFF41
#define SCY 0xFF42
#define SCX 0xFF43
#define LY 0xFF44
#define LYC 0xFF45
#define DMA 0xFF46
#define BGP 0xFF47
#define OBP0 0xFF48
#define OBP1 0xFF49
#define WY 0xFF4A
#define WX 0xFF4B
#define UNK1 0xFF4C
#define KEY1 0xFF4D
#define VBK 0xFF4F
#define BIOS 0xFF50
#define HDMA1 0xFF51
#define HDMA2 0xFF52
#define HDMA3 0xFF53
#define HDMA4 0xFF54
#define HDMA5 0xFF55
#define RP 0xFF56
#define BCPS 0xFF68
#define BCPD 0xFF69
#define OCPS 0xFF6A
#define OCPD 0xFF6B
#define UNK2 0xFF6C
#define SVBK 0xFF70
#define UNK3 0xFF72
#define UNK4 0xFF73
#define UNK5 0xFF74
#define UNK6 0xFF75
#define UNK7 0xFF76
#define UNK8 0xFF77
#define IE 0xFFFF

class MMU {
public:
    MMU(Gameboy* gameboy);

    void reset();

    void loadState(FILE* file, int version);
    void saveState(FILE* file);

    u8 read(u16 addr);
    void write(u16 addr, u8 val);

    void mapBank(u8 bank, u8* block);
    void mapBankReadFunc(u8 bank, std::function<u8(u16 addr)> readFunc);
    void mapBankWriteFunc(u8 bank, std::function<void(u16 addr, u8 val)> writeFunc);
    void unmapBank(u8 bank);

    void mapIOReadFunc(u16 addr, std::function<u8(u16 addr)> readFunc);
    void mapIOWriteFunc(u16 addr, std::function<void(u16 addr, u8 val)> writeFunc);
    void unmapIO(u16 addr);

    inline u8 readIO(u16 addr) {
        return this->hram[addr & 0xFF];
    }

    inline void writeIO(u16 addr, u8 val) {
        this->hram[addr & 0xFF] = val;
    }
private:
    void mapBanks();

    Gameboy* gameboy;

    u8* banks[0x10];
    std::function<u8(u16 addr)> bankReadFuncs[0x10];
    std::function<void(u16 addr, u8 val)> bankWriteFuncs[0x10];

    std::function<u8(u16 addr)> ioReadFuncs[0x100];
    std::function<void(u16 addr, u8 val)> ioWriteFuncs[0x100];

    u8 wram[8][0x1000];
    u8 hram[0x100];
};