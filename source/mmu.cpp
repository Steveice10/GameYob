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
            case JOYP:
                return this->gameboy->sgb->read(addr);
            case SB:
            case SC:
                return this->gameboy->serial->read(addr);
            case DIV:
            case TIMA:
            case TMA:
            case TAC:
                return this->gameboy->timer->read(addr);
            case IF:
            case KEY1:
            case IE:
                return this->gameboy->cpu->read(addr);
            case NR10:
            case NR11:
            case NR12:
            case NR13:
            case NR14:
            case NR20:
            case NR21:
            case NR22:
            case NR23:
            case NR24:
            case NR30:
            case NR31:
            case NR32:
            case NR33:
            case NR34:
            case NR40:
            case NR41:
            case NR42:
            case NR43:
            case NR44:
            case NR50:
            case NR51:
            case NR52:
            case WAVE0:
            case WAVE1:
            case WAVE2:
            case WAVE3:
            case WAVE4:
            case WAVE5:
            case WAVE6:
            case WAVE7:
            case WAVE8:
            case WAVE9:
            case WAVEA:
            case WAVEB:
            case WAVEC:
            case WAVED:
            case WAVEE:
            case WAVEF:
                return this->gameboy->apu->read(addr);
            case LCDC:
            case STAT:
            case SCY:
            case SCX:
            case LY:
            case LYC:
            case DMA:
            case BGP:
            case OBP0:
            case OBP1:
            case WY:
            case WX:
            case VBK:
            case HDMA1:
            case HDMA2:
            case HDMA3:
            case HDMA4:
            case HDMA5:
            case BCPS:
            case BCPD:
            case OCPS:
            case OCPD:
                return this->gameboy->ppu->read(addr);
            case RP:
                return this->gameboy->ir->read(addr);
            case BIOS:
                return (u8) (this->gameboy->biosOn ? 0 : 1);
            case SVBK:
                return (u8) (this->wramBank | 0xF8);
            case UNK1:
            case UNK2:
            case UNK3:
            case UNK4:
            case UNK5:
            case UNK6:
            case UNK7:
            case UNK8:
                return 0;
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
            case JOYP:
                this->gameboy->sgb->write(addr, val);
                break;
            case SB:
            case SC:
                this->gameboy->serial->write(addr, val);
                break;
            case DIV:
            case TIMA:
            case TMA:
            case TAC:
                this->gameboy->timer->write(addr, val);
                break;
            case IF:
            case KEY1:
            case IE:
                this->gameboy->cpu->write(addr, val);
                break;
            case NR10:
            case NR11:
            case NR12:
            case NR13:
            case NR14:
            case NR20:
            case NR21:
            case NR22:
            case NR23:
            case NR24:
            case NR30:
            case NR31:
            case NR32:
            case NR33:
            case NR34:
            case NR40:
            case NR41:
            case NR42:
            case NR43:
            case NR44:
            case NR50:
            case NR51:
            case NR52:
            case WAVE0:
            case WAVE1:
            case WAVE2:
            case WAVE3:
            case WAVE4:
            case WAVE5:
            case WAVE6:
            case WAVE7:
            case WAVE8:
            case WAVE9:
            case WAVEA:
            case WAVEB:
            case WAVEC:
            case WAVED:
            case WAVEE:
            case WAVEF:
                this->gameboy->apu->write(addr, val);
                break;
            case LCDC:
            case STAT:
            case SCY:
            case SCX:
            case LY:
            case LYC:
            case DMA:
            case BGP:
            case OBP0:
            case OBP1:
            case WY:
            case WX:
            case VBK:
            case HDMA1:
            case HDMA2:
            case HDMA3:
            case HDMA4:
            case HDMA5:
            case BCPS:
            case BCPD:
            case OCPS:
            case OCPD:
                this->gameboy->ppu->write(addr, val);
                break;
            case RP:
                this->gameboy->ir->write(addr, val);
                break;
            case BIOS:
                if(this->gameboy->biosOn) {
                    this->gameboy->reset(false);
                }

                break;
            case SVBK:
                if(this->gameboy->gbMode == MODE_CGB) {
                    this->wramBank = (u8) (val & 7);
                    if(this->wramBank == 0) {
                        this->wramBank = 1;
                    }

                    this->mapBanks();
                }

                break;
            case UNK1:
            case UNK2:
            case UNK3:
            case UNK4:
            case UNK5:
            case UNK6:
            case UNK7:
            case UNK8:
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