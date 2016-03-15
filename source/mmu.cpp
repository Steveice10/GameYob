#include <stdio.h>
#include <string.h>

#include "platform/common/manager.h"
#include "platform/system.h"
#include "apu.h"
#include "cpu.h"
#include "gameboy.h"
#include "mbc.h"
#include "mmu.h"
#include "ppu.h"

MMU::MMU(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

void MMU::reset() {
    memset(this->banks, 0, sizeof(this->banks));
    memset(this->bankReadFuncs, 0, sizeof(this->bankReadFuncs));
    memset(this->bankWriteFuncs, 0, sizeof(this->bankWriteFuncs));

    memset(this->ioReadFuncs, 0, sizeof(this->ioReadFuncs));
    memset(this->ioWriteFuncs, 0, sizeof(this->ioWriteFuncs));

    for(int i = 0; i < 8; i++) {
        memset(this->wram[i], 0, sizeof(this->wram[i]));
    }

    memset(this->hram, 0, sizeof(this->hram));

    this->writeIO(SVBK, 1);

    this->mapBanks();

    this->gameboy->mmu->mapIOReadFunc(RP, [this](u16 addr, u8 val) -> u8 {
        return val | (u8) ((val & 0x80) && systemGetIRState() ? 0x0 : 0x2);
    });

    this->gameboy->mmu->mapIOReadFunc(SVBK, [this](u16 addr, u8 val) -> u8 {
        return (u8) (val | 0xF8);
    });

    this->gameboy->mmu->mapIOWriteFunc(BIOS, [this](u16 addr, u8 val) -> u8 {
        if(this->gameboy->biosOn) {
            this->gameboy->reset(false);
        }

        return val;
    });

    this->gameboy->mmu->mapIOWriteFunc(RP, [this](u16 addr, u8 val) -> u8 {
        systemSetIRState((val & 0x1) == 1);
        return (u8) (val & ~0x2);
    });

    this->gameboy->mmu->mapIOWriteFunc(SVBK, [this](u16 addr, u8 val) -> u8 {
        if(this->gameboy->gbMode == MODE_CGB) {
            val = (u8) (val & 7);
            if(val == 0) {
                val = 1;
            }

            this->mapBank(0xD, this->wram[val]);
        }

        return val;
    });
}

void MMU::loadState(FILE* file, int version) {
    fread(this->wram, 1, sizeof(this->wram), file);
    fread(this->hram, 1, sizeof(this->hram), file);

    this->mapBanks();
}

void MMU::saveState(FILE* file) {
    fwrite(this->wram, 1, sizeof(this->wram), file);
    fwrite(this->hram, 1, sizeof(this->hram), file);
}

u8 MMU::read(u16 addr) {
    int area = addr >> 12;
    if(this->bankReadFuncs[area] != NULL) {
        return this->bankReadFuncs[area](addr);
    } else if(this->banks[area] != NULL) {
        return this->banks[area][addr & 0xFFF];
    } else {
        systemPrintDebug("Attempted to read from unmapped memory bank: 0x%x\n", area);
        return 0xFF;
    }
}

void MMU::write(u16 addr, u8 val) {
    int area = addr >> 12;
    if(this->bankWriteFuncs[area] != NULL) {
        this->bankWriteFuncs[area](addr, val);
    } else if(this->banks[area] != NULL) {
        this->banks[area][addr & 0xFFF] = val;
    } else {
        systemPrintDebug("Attempted to write to unmapped memory bank: 0x%x\n", area);
    }
}

void MMU::mapBank(u8 bank, u8* block) {
    this->banks[bank & 0xF] = block;
}

void MMU::mapBankReadFunc(u8 bank, std::function<u8(u16 addr)> readFunc) {
    this->bankReadFuncs[bank & 0xF] = readFunc;
}

void MMU::mapBankWriteFunc(u8 bank, std::function<void(u16 addr, u8 val)> writeFunc) {
    this->bankWriteFuncs[bank & 0xF] = writeFunc;
}

void MMU::unmapBank(u8 bank) {
    this->banks[bank & 0xF] = NULL;
    this->bankReadFuncs[bank & 0xF] = NULL;
    this->bankWriteFuncs[bank & 0xF] = NULL;
}

void MMU::mapBanks() {
    this->mapBank(0xC, this->wram[0]);
    this->mapBank(0xD, this->wram[this->readIO(SVBK)]);
    this->mapBank(0xE, this->wram[0]);

    this->mapBankReadFunc(0xF, [this](u16 addr) -> u8 {
        if(addr >= 0xFF00) {
            u8 reg = (u8) (addr & 0xFF);
            u8 val = this->hram[reg];
            if(this->ioReadFuncs[reg] != NULL) {
                val = this->ioReadFuncs[reg](addr, val);
            }

            return val;
        } else if(addr >= 0xFE00 && addr < 0xFEA0) {
            return this->gameboy->ppu->getOam()[addr & 0xFF];
        } else if(addr < 0xFE00) {
            return this->wram[this->readIO(SVBK)][addr & 0xFFF];
        }

        return 0;
    });

    this->mapBankWriteFunc(0xF, [this](u16 addr, u8 val) -> void {
        if(addr >= 0xFF00) {
            u8 reg = (u8) (addr & 0xFF);
            if(this->ioWriteFuncs[reg] != NULL) {
                val = this->ioWriteFuncs[reg](addr, val);
            }

            this->hram[reg] = val;
        } else if(addr >= 0xFE00 && addr < 0xFEA0) {
            this->gameboy->ppu->getOam()[addr & 0xFF] = val;
        } else if(addr < 0xFE00) {
            this->wram[this->readIO(SVBK)][addr & 0xFFF] = val;
        }
    });
}

void MMU::mapIOReadFunc(u16 addr, std::function<u8(u16 addr, u8 val)> readFunc) {
    this->ioReadFuncs[addr & 0xFF] = readFunc;
}

void MMU::mapIOWriteFunc(u16 addr, std::function<u8(u16 addr, u8 val)> writeFunc) {
    this->ioWriteFuncs[addr & 0xFF] = writeFunc;
}

void MMU::unmapIO(u16 addr) {
    this->ioReadFuncs[addr & 0xFF] = NULL;
    this->ioWriteFuncs[addr & 0xFF] = NULL;
}