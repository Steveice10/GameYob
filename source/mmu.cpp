#include <stdio.h>
#include <string.h>

#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/system.h"
#include "apu.h"
#include "cpu.h"
#include "gameboy.h"
#include "ir.h"
#include "mbc.h"
#include "mmu.h"
#include "ppu.h"
#include "sgb.h"
#include "serial.h"
#include "timer.h"

MMU::MMU(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

void MMU::reset() {
    memset(this->mappedBlocks, 0, sizeof(this->mappedBlocks));
    memset(this->mappedReadFuncs, 0, sizeof(this->mappedReadFuncs));
    memset(this->mappedReadFuncData, 0, sizeof(this->mappedReadFuncData));
    memset(this->mappedWriteFuncs, 0, sizeof(this->mappedWriteFuncs));
    memset(this->mappedWriteFuncData, 0, sizeof(this->mappedWriteFuncData));

    this->wramBank = 1;
    for(int i = 0; i < 8; i++) {
        memset(this->wram[i], 0, sizeof(this->wram[i]));
    }

    memset(this->hram, 0, sizeof(this->hram));

    this->mapBanks();
}

void MMU::loadState(FILE* file, int version) {
    fread(&this->wramBank, 1, sizeof(this->wramBank), file);
    fread(this->wram, 1, sizeof(this->wram), file);
    fread(this->hram, 1, sizeof(this->hram), file);

    this->mapBanks();
}

void MMU::saveState(FILE* file) {
    fwrite(&this->wramBank, 1, sizeof(this->wramBank), file);
    fwrite(this->wram, 1, sizeof(this->wram), file);
    fwrite(this->hram, 1, sizeof(this->hram), file);
}

u8 MMU::read(u16 addr) {
    int area = addr >> 12;
    if(this->mappedReadFuncs[area] != NULL) {
        return this->mappedReadFuncs[area](this->mappedReadFuncData[area], addr);
    } else if(this->mappedBlocks[area] != NULL) {
        return this->mappedBlocks[area][addr & 0xFFF];
    } else {
        systemPrintDebug("Attempted to read from unmapped memory bank: 0x%x\n", area);
        return 0xFF;
    }
}

void MMU::write(u16 addr, u8 val) {
    int area = addr >> 12;
    if(this->mappedWriteFuncs[area] != NULL) {
        this->mappedWriteFuncs[area](this->mappedWriteFuncData[area], addr, val);
    } else if(this->mappedBlocks[area] != NULL) {
        this->mappedBlocks[area][addr & 0xFFF] = val;
    } else {
        systemPrintDebug("Attempted to write to unmapped memory bank: 0x%x\n", area);
    }
}

u8 MMU::readBankF(u16 addr) {
    if(addr >= 0xFF80 && addr != 0xFFFF) {
        return this->hram[addr & 0x7F];
    } else if(addr >= 0xFF00) {
        switch(addr) {
            case 0xFF00:
                return this->gameboy->sgb->read(addr);
            case 0xFF01:
            case 0xFF02:
                return this->gameboy->serial->read(addr);
            case 0xFF04:
            case 0xFF05:
            case 0xFF06:
            case 0xFF07:
                return this->gameboy->timer->read(addr);
            case 0xFF0F:
            case 0xFF4D:
            case 0xFFFF:
                return this->gameboy->cpu->read(addr);
            case 0xFF10:
            case 0xFF11:
            case 0xFF12:
            case 0xFF13:
            case 0xFF14:
            case 0xFF15:
            case 0xFF16:
            case 0xFF17:
            case 0xFF18:
            case 0xFF19:
            case 0xFF1A:
            case 0xFF1B:
            case 0xFF1C:
            case 0xFF1D:
            case 0xFF1E:
            case 0xFF1F:
            case 0xFF20:
            case 0xFF21:
            case 0xFF22:
            case 0xFF23:
            case 0xFF24:
            case 0xFF25:
            case 0xFF26:
            case 0xFF27:
            case 0xFF28:
            case 0xFF29:
            case 0xFF2A:
            case 0xFF2B:
            case 0xFF2C:
            case 0xFF2D:
            case 0xFF2E:
            case 0xFF2F:
            case 0xFF30:
            case 0xFF31:
            case 0xFF32:
            case 0xFF33:
            case 0xFF34:
            case 0xFF35:
            case 0xFF36:
            case 0xFF37:
            case 0xFF38:
            case 0xFF39:
            case 0xFF3A:
            case 0xFF3B:
            case 0xFF3C:
            case 0xFF3D:
            case 0xFF3E:
            case 0xFF3F:
                return this->gameboy->apu->read(addr);
            case 0xFF40:
            case 0xFF41:
            case 0xFF42:
            case 0xFF43:
            case 0xFF44:
            case 0xFF45:
            case 0xFF46:
            case 0xFF47:
            case 0xFF48:
            case 0xFF49:
            case 0xFF4A:
            case 0xFF4B:
            case 0xFF4C:
            case 0xFF4F:
            case 0xFF51:
            case 0xFF52:
            case 0xFF53:
            case 0xFF54:
            case 0xFF55:
            case 0xFF68:
            case 0xFF69:
            case 0xFF6A:
            case 0xFF6B:
            case 0xFF6C:
                return this->gameboy->ppu->read(addr);
            case 0xFF56:
                return this->gameboy->ir->read(addr);
            case 0xFF50:
                return (u8) (this->gameboy->biosOn ? 0 : 1);
            case 0xFF70:
                return (u8) (this->wramBank | 0xF8);
            default:
                systemPrintDebug("Unimplemented IO register read: 0x%x\n", addr);
                return 0;
        }
    } else if(addr >= 0xFE00 && addr < 0xFEA0) {
        return this->gameboy->ppu->read(addr);
    } else if(addr < 0xFE00) {
        return this->wram[this->wramBank][addr & 0xFFF];
    }

    return 0;
}

void MMU::writeBankF(u16 addr, u8 val) {
    if(addr >= 0xFF80 && addr != 0xFFFF) {
        this->hram[addr & 0x7F] = val;
    } else if(addr >= 0xFF00) {
        switch(addr) {
            case 0xFF00:
                this->gameboy->sgb->write(addr, val);
                break;
            case 0xFF01:
            case 0xFF02:
                this->gameboy->serial->write(addr, val);
                break;
            case 0xFF04:
            case 0xFF05:
            case 0xFF06:
            case 0xFF07:
                this->gameboy->timer->write(addr, val);
                break;
            case 0xFF0F:
            case 0xFF4D:
            case 0xFFFF:
                this->gameboy->cpu->write(addr, val);
                break;
            case 0xFF10:
            case 0xFF11:
            case 0xFF12:
            case 0xFF13:
            case 0xFF14:
            case 0xFF15:
            case 0xFF16:
            case 0xFF17:
            case 0xFF18:
            case 0xFF19:
            case 0xFF1A:
            case 0xFF1B:
            case 0xFF1C:
            case 0xFF1D:
            case 0xFF1E:
            case 0xFF1F:
            case 0xFF20:
            case 0xFF21:
            case 0xFF22:
            case 0xFF23:
            case 0xFF24:
            case 0xFF25:
            case 0xFF26:
            case 0xFF27:
            case 0xFF28:
            case 0xFF29:
            case 0xFF2A:
            case 0xFF2B:
            case 0xFF2C:
            case 0xFF2D:
            case 0xFF2E:
            case 0xFF2F:
            case 0xFF30:
            case 0xFF31:
            case 0xFF32:
            case 0xFF33:
            case 0xFF34:
            case 0xFF35:
            case 0xFF36:
            case 0xFF37:
            case 0xFF38:
            case 0xFF39:
            case 0xFF3A:
            case 0xFF3B:
            case 0xFF3C:
            case 0xFF3D:
            case 0xFF3E:
            case 0xFF3F:
                this->gameboy->apu->write(addr, val);
                break;
            case 0xFF40:
            case 0xFF41:
            case 0xFF42:
            case 0xFF43:
            case 0xFF44:
            case 0xFF45:
            case 0xFF46:
            case 0xFF47:
            case 0xFF48:
            case 0xFF49:
            case 0xFF4A:
            case 0xFF4B:
            case 0xFF4C:
            case 0xFF4F:
            case 0xFF51:
            case 0xFF52:
            case 0xFF53:
            case 0xFF54:
            case 0xFF55:
            case 0xFF68:
            case 0xFF69:
            case 0xFF6A:
            case 0xFF6B:
            case 0xFF6C:
                this->gameboy->ppu->write(addr, val);
                break;
            case 0xFF56:
                this->gameboy->ir->write(addr, val);
                break;
            case 0xFF50:
                if(this->gameboy->biosOn) {
                    this->gameboy->reset(false);
                }

                break;
            case 0xFF70:
                if(this->gameboy->gbMode == MODE_CGB) {
                    this->wramBank = (u8) (val & 7);
                    if(this->wramBank == 0) {
                        this->wramBank = 1;
                    }

                    this->mapBanks();
                }

                break;
            default:
                systemPrintDebug("Unimplemented IO register write: 0x%x, 0x%x\n", addr, val);
                break;
        }
    } else if(addr >= 0xFE00 && addr < 0xFEA0) {
        this->gameboy->ppu->write(addr, val);
    } else if(addr < 0xFE00) {
        this->wram[this->wramBank][addr & 0xFFF] = val;
    }
}

u8 MMU::readBankFEntry(void* data, u16 addr) {
    return ((MMU*) data)->readBankF(addr);
}

void MMU::writeBankFEntry(void* data, u16 addr, u8 val) {
    ((MMU*) data)->writeBankF(addr, val);
}

void MMU::mapBlock(u8 bank, u8* block) {
    this->mappedBlocks[bank & 0xF] = block;
}

void MMU::mapReadFunc(u8 bank, void* data, u8 (*readFunc)(void* data, u16 addr)) {
    this->mappedReadFuncs[bank & 0xF] = readFunc;
    this->mappedReadFuncData[bank & 0xF] = data;
}

void MMU::mapWriteFunc(u8 bank, void* data, void (*writeFunc)(void* data, u16 addr, u8 val)) {
    this->mappedWriteFuncs[bank & 0xF] = writeFunc;
    this->mappedWriteFuncData[bank & 0xF] = data;
}

void MMU::unmap(u8 bank) {
    this->mappedBlocks[bank & 0xF] = NULL;
    this->mappedReadFuncs[bank & 0xF] = NULL;
    this->mappedReadFuncData[bank & 0xF] = NULL;
    this->mappedWriteFuncs[bank & 0xF] = NULL;
    this->mappedWriteFuncData[bank & 0xF] = NULL;
}

void MMU::mapBanks() {
    this->mapBlock(0xC, this->wram[0]);
    this->mapBlock(0xD, this->wram[this->wramBank]);
    this->mapBlock(0xE, this->wram[0]);

    this->mapReadFunc(0xF, this, MMU::readBankFEntry);
    this->mapWriteFunc(0xF, this, MMU::writeBankFEntry);
}