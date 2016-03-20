#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <platform/common/manager.h>
#include <platform/common/menu.h>

#include "platform/input.h"
#include "platform/system.h"
#include "cpu.h"
#include "gameboy.h"
#include "mbc.h"
#include "mmu.h"
#include "romfile.h"

MBC::MBC(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

MBC::~MBC() {
    if(this->sram != NULL) {
        delete this->sram;
        this->sram = NULL;
    }
}

void MBC::reset() {
    if(this->sram != NULL) {
        delete this->sram;
        this->sram = NULL;
    }

    this->readFunc = this->mbcReads[this->gameboy->romFile->getMBCType()];
    this->writeFunc = this->mbcWrites[this->gameboy->romFile->getMBCType()];
    this->updateFunc = this->mbcUpdates[this->gameboy->romFile->getMBCType()];

    this->sram = new u8[this->gameboy->romFile->getRamBanks() * 0x2000]();
    memset(&this->gbClock, 0, sizeof(this->gbClock));

    // General
    this->romBank0 = 0;
    this->romBank1 = 1;
    this->sramBank = 0;
    this->sramEnabled = false;

    // MBC1
    this->mbc1RockmanMapper = this->gameboy->romFile->getRomTitle().compare("ROCKMAN 99") == 0;
    this->mbc1RamMode = false;

    // MBC6
    this->romBank1ALatch = 2;
    this->romBank1BLatch = 3;
    this->romBank1A = 2;
    this->romBank1B = 3;

    // MBC7
    this->mbc7WriteEnable = false;
    this->mbc7Cs = 0;
    this->mbc7Sk = 0;
    this->mbc7OpCode = 0;
    this->mbc7Addr = 0;
    this->mbc7Count = 0;
    this->mbc7State = 0;
    this->mbc7Buffer = 0;
    this->mbc7RA = 0;

    // HuC1
    this->huc1RamMode = false;

    // HuC3
    this->huc3Mode = 0;
    this->huc3Value = 0;
    this->huc3Shift = 0;

    // MMM01
    this->mmm01BankSelected = false;
    this->mmm01RomBaseBank = 0;

    // CAMERA
    this->cameraIO = false;
    this->cameraReadyCycle = 0;
    memset(this->cameraRegs, 0, sizeof(this->cameraRegs));

    // TAMA5
    this->tama5CommandNumber = 0;
    this->tama5RamByteSelect = 0;
    memset(this->tama5Commands, 0, sizeof(this->tama5Commands));
    memset(this->tama5RAM, 0, sizeof(this->tama5RAM));

    this->mapBanks();
}

void MBC::loadState(FILE* file, int version) {
    fread(this->sram, 1, sizeof(this->sram), file);
    fread(&this->gbClock, 1, sizeof(this->gbClock), file);

    fread(&this->romBank0, 1, sizeof(this->romBank0), file);
    fread(&this->romBank1, 1, sizeof(this->romBank1), file);
    fread(&this->sramBank, 1, sizeof(this->sramBank), file);
    fread(&this->sramEnabled, 1, sizeof(this->sramEnabled), file);

    switch(this->gameboy->romFile->getMBCType()) {
        case MBC1:
            fread(&this->mbc1RockmanMapper, 1, sizeof(this->mbc1RockmanMapper), file);
            fread(&this->mbc1RamMode, 1, sizeof(this->mbc1RamMode), file);
            break;
        case MBC6:
            fread(&this->romBank1ALatch, 1, sizeof(this->romBank1ALatch), file);
            fread(&this->romBank1BLatch, 1, sizeof(this->romBank1BLatch), file);
            fread(&this->romBank1A, 1, sizeof(this->romBank1A), file);
            fread(&this->romBank1B, 1, sizeof(this->romBank1B), file);
            break;
        case MBC7:
            fread(&this->mbc7WriteEnable, 1, sizeof(this->mbc7WriteEnable), file);
            fread(&this->mbc7Cs, 1, sizeof(this->mbc7Cs), file);
            fread(&this->mbc7Sk, 1, sizeof(this->mbc7Sk), file);
            fread(&this->mbc7OpCode, 1, sizeof(this->mbc7OpCode), file);
            fread(&this->mbc7Addr, 1, sizeof(this->mbc7Addr), file);
            fread(&this->mbc7Cs, 1, sizeof(this->mbc7Cs), file);
            fread(&this->mbc7Count, 1, sizeof(this->mbc7Count), file);
            fread(&this->mbc7State, 1, sizeof(this->mbc7State), file);
            fread(&this->mbc7Buffer, 1, sizeof(this->mbc7Buffer), file);
            fread(&this->mbc7RA, 1, sizeof(this->mbc7RA), file);
            break;
        case HUC1:
            fread(&this->huc1RamMode, 1, sizeof(this->huc1RamMode), file);
            break;
        case HUC3:
            fread(&this->huc3Mode, 1, sizeof(this->huc3Mode), file);
            fread(&this->huc3Value, 1, sizeof(this->huc3Value), file);
            fread(&this->huc3Shift, 1, sizeof(this->huc3Shift), file);
            break;
        case MMM01:
            fread(&this->mmm01BankSelected, 1, sizeof(this->mmm01BankSelected), file);
            fread(&this->mmm01RomBaseBank, 1, sizeof(this->mmm01RomBaseBank), file);
            break;
        case CAMERA:
            fread(&this->cameraIO, 1, sizeof(this->cameraIO), file);
            fread(&this->cameraReadyCycle, 1, sizeof(this->cameraReadyCycle), file);
            fread(this->cameraRegs, 1, sizeof(this->cameraRegs), file);
            break;
        case TAMA5:
            fread(&this->tama5CommandNumber, 1, sizeof(this->tama5CommandNumber), file);
            fread(&this->tama5RamByteSelect, 1, sizeof(this->tama5RamByteSelect), file);
            fread(this->tama5Commands, 1, sizeof(this->tama5Commands), file);
            fread(this->tama5RAM, 1, sizeof(this->tama5RAM), file);
        default:
            break;
    }

    this->mapBanks();
}

void MBC::saveState(FILE* file) {
    fwrite(this->sram, 1, sizeof(this->sram), file);
    fwrite(&this->gbClock, 1, sizeof(this->gbClock), file);

    fwrite(&this->romBank0, 1, sizeof(this->romBank0), file);
    fwrite(&this->romBank1, 1, sizeof(this->romBank1), file);
    fwrite(&this->sramBank, 1, sizeof(this->sramBank), file);
    fwrite(&this->sramEnabled, 1, sizeof(this->sramEnabled), file);

    switch(this->gameboy->romFile->getMBCType()) {
        case MBC1:
            fwrite(&this->mbc1RockmanMapper, 1, sizeof(this->mbc1RockmanMapper), file);
            fwrite(&this->mbc1RamMode, 1, sizeof(this->mbc1RamMode), file);
            break;
        case MBC6:
            fwrite(&this->romBank1ALatch, 1, sizeof(this->romBank1ALatch), file);
            fwrite(&this->romBank1BLatch, 1, sizeof(this->romBank1BLatch), file);
            fwrite(&this->romBank1A, 1, sizeof(this->romBank1A), file);
            fwrite(&this->romBank1B, 1, sizeof(this->romBank1B), file);
            break;
        case MBC7:
            fwrite(&this->mbc7WriteEnable, 1, sizeof(this->mbc7WriteEnable), file);
            fwrite(&this->mbc7Cs, 1, sizeof(this->mbc7Cs), file);
            fwrite(&this->mbc7Sk, 1, sizeof(this->mbc7Sk), file);
            fwrite(&this->mbc7OpCode, 1, sizeof(this->mbc7OpCode), file);
            fwrite(&this->mbc7Addr, 1, sizeof(this->mbc7Addr), file);
            fwrite(&this->mbc7Cs, 1, sizeof(this->mbc7Cs), file);
            fwrite(&this->mbc7Count, 1, sizeof(this->mbc7Count), file);
            fwrite(&this->mbc7State, 1, sizeof(this->mbc7State), file);
            fwrite(&this->mbc7Buffer, 1, sizeof(this->mbc7Buffer), file);
            fwrite(&this->mbc7RA, 1, sizeof(this->mbc7RA), file);
            break;
        case HUC1:
            fwrite(&this->huc1RamMode, 1, sizeof(this->huc1RamMode), file);
            break;
        case HUC3:
            fwrite(&this->huc3Mode, 1, sizeof(this->huc3Mode), file);
            fwrite(&this->huc3Value, 1, sizeof(this->huc3Value), file);
            fwrite(&this->huc3Shift, 1, sizeof(this->huc3Shift), file);
            break;
        case MMM01:
            fwrite(&this->mmm01BankSelected, 1, sizeof(this->mmm01BankSelected), file);
            fwrite(&this->mmm01RomBaseBank, 1, sizeof(this->mmm01RomBaseBank), file);
            break;
        case CAMERA:
            fwrite(&this->cameraIO, 1, sizeof(this->cameraIO), file);
            fwrite(&this->cameraReadyCycle, 1, sizeof(this->cameraReadyCycle), file);
            fwrite(this->cameraRegs, 1, sizeof(this->cameraRegs), file);
            break;
        case TAMA5:
            fwrite(&this->tama5CommandNumber, 1, sizeof(this->tama5CommandNumber), file);
            fwrite(&this->tama5RamByteSelect, 1, sizeof(this->tama5RamByteSelect), file);
            fwrite(this->tama5Commands, 1, sizeof(this->tama5Commands), file);
            fwrite(this->tama5RAM, 1, sizeof(this->tama5RAM), file);
        default:
            break;
    }
}

void MBC::update() {
    if(this->updateFunc != NULL) {
        (this->*updateFunc)();
    }
}

u32 MBC::getSaveSize() {
    u32 size = (u32) (this->gameboy->romFile->getRamBanks() * 0x2000);
    if(this->gameboy->romFile->getMBCType() == MBC3 || this->gameboy->romFile->getMBCType() == HUC3 || this->gameboy->romFile->getMBCType() == TAMA5) {
        size += sizeof(this->gbClock);
    }

    return size;
}

u32 MBC::load(u8* buffer, u32 size) {
    u32 remaining = size;

    u32 saveSize = (u32) (this->gameboy->romFile->getRamBanks() * 0x2000);
    u32 sramSize = remaining < saveSize ? remaining : saveSize;
    memcpy(this->sram, buffer, sramSize);
    remaining -= sramSize;

    if(this->gameboy->romFile->getMBCType() == MBC3 || this->gameboy->romFile->getMBCType() == HUC3 || this->gameboy->romFile->getMBCType() == TAMA5) {
        u32 gbClockSize = remaining < sizeof(this->gbClock) ? remaining : sizeof(this->gbClock);
        memcpy(&this->gbClock, buffer + sramSize, gbClockSize);
        remaining -= sramSize;
    }

    return size - remaining;
}

u32 MBC::save(u8* buffer, u32 size) {
    u32 remaining = size;

    u32 saveSize = (u32) (this->gameboy->romFile->getRamBanks() * 0x2000);
    u32 sramSize = remaining < saveSize ? remaining : saveSize;
    memcpy(buffer, this->sram, sramSize);
    remaining -= sramSize;

    if(this->gameboy->romFile->getMBCType() == MBC3 || this->gameboy->romFile->getMBCType() == HUC3 || this->gameboy->romFile->getMBCType() == TAMA5) {
        u32 gbClockSize = remaining < sizeof(this->gbClock) ? remaining : sizeof(this->gbClock);
        memcpy(buffer + sramSize, &this->gbClock, gbClockSize);
        remaining -= sramSize;
    }

    return size - remaining;
}

u8 MBC::readSram(u16 addr) {
    if(this->sramBank >= 0 && this->sramBank < this->gameboy->romFile->getRamBanks()) {
        return this->sram[this->sramBank * 0x2000 + (addr & 0x1FFF)];
    } else {
        systemPrintDebug("Attempted to read from invalid SRAM bank: %d\n", this->sramBank);
        return 0xFF;
    }
}

void MBC::writeSram(u16 addr, u8 val) {
    if(this->sramBank >= 0 && this->sramBank < this->gameboy->romFile->getRamBanks()) {
        this->sram[this->sramBank * 0x2000 + (addr & 0x1FFF)] = val;
    } else {
        systemPrintDebug("Attempted to write to invalid SRAM bank: %d\n", this->sramBank);
    }
}

void MBC::mapRomBank0() {
    u8* bankPtr = this->gameboy->romFile->getRomBank(this->romBank0);
    if(bankPtr != NULL) {
        this->gameboy->mmu->mapBank(0x0, bankPtr + 0x0000);
        this->gameboy->mmu->mapBank(0x1, bankPtr + 0x1000);
        this->gameboy->mmu->mapBank(0x2, bankPtr + 0x2000);
        this->gameboy->mmu->mapBank(0x3, bankPtr + 0x3000);
    } else {
        systemPrintDebug("Attempted to access invalid ROM bank: %d\n", this->romBank0);

        this->gameboy->mmu->mapBank(0x0, NULL);
        this->gameboy->mmu->mapBank(0x1, NULL);
        this->gameboy->mmu->mapBank(0x2, NULL);
        this->gameboy->mmu->mapBank(0x3, NULL);
    }

    if(this->gameboy->biosOn) {
        u8* bios = gbcBios;
        if(biosMode == 1) {
            bios = this->gameboy->gbMode != MODE_CGB && gbBiosLoaded ? (u8*) gbBios : (u8*) gbcBios;
        } else if(biosMode == 2) {
            bios = gbBios;
        } else if(biosMode == 3) {
            bios = gbcBios;
        }

        memcpy(bios + 0x100, this->gameboy->romFile->getRomBank(0) + 0x100, 0x100);

        this->gameboy->mmu->mapBank(0x0, bios);
    }
}

void MBC::mapRomBank1() {
    if(this->gameboy->romFile->getMBCType() == MBC6) {
        u8* bankPtr1A = this->gameboy->romFile->getRomBank(this->romBank1A >> 1);
        if(bankPtr1A != NULL) {
            u8* subPtr = bankPtr1A + (this->romBank1A & 0x1) * 0x2000;

            this->gameboy->mmu->mapBank(0x4, subPtr + 0x0000);
            this->gameboy->mmu->mapBank(0x5, subPtr + 0x1000);
        } else {
            systemPrintDebug("Attempted to access invalid sub ROM bank: %d\n", this->romBank1A);

            this->gameboy->mmu->mapBank(0x4, NULL);
            this->gameboy->mmu->mapBank(0x5, NULL);
        }

        u8* bankPtr1B = this->gameboy->romFile->getRomBank(this->romBank1B >> 1);
        if(bankPtr1B != NULL) {
            u8* subPtr = bankPtr1B + (this->romBank1B & 0x1) * 0x2000;

            this->gameboy->mmu->mapBank(0x6, subPtr + 0x0000);
            this->gameboy->mmu->mapBank(0x7, subPtr + 0x1000);
        } else {
            systemPrintDebug("Attempted to access invalid sub ROM bank: %d\n", this->romBank1B);

            this->gameboy->mmu->mapBank(0x6, NULL);
            this->gameboy->mmu->mapBank(0x7, NULL);
        }
    } else {
        u8* bankPtr = this->gameboy->romFile->getRomBank(this->romBank1);
        if(bankPtr != NULL) {
            this->gameboy->mmu->mapBank(0x4, bankPtr + 0x0000);
            this->gameboy->mmu->mapBank(0x5, bankPtr + 0x1000);
            this->gameboy->mmu->mapBank(0x6, bankPtr + 0x2000);
            this->gameboy->mmu->mapBank(0x7, bankPtr + 0x3000);
        } else {
            systemPrintDebug("Attempted to access invalid ROM bank: %d\n", this->romBank1);

            this->gameboy->mmu->mapBank(0x4, NULL);
            this->gameboy->mmu->mapBank(0x5, NULL);
            this->gameboy->mmu->mapBank(0x6, NULL);
            this->gameboy->mmu->mapBank(0x7, NULL);
        }
    }
}

void MBC::mapSramBank() {
    if(this->sramBank >= 0 && this->sramBank < this->gameboy->romFile->getRamBanks()) {
        u8* bankPtr = &this->sram[this->sramBank * 0x2000];

        this->gameboy->mmu->mapBank(0xA, bankPtr + 0x0000);
        this->gameboy->mmu->mapBank(0xB, bankPtr + 0x1000);
    } else {
        // Only report if there's no chance it's a special bank number.
        if(this->readFunc == NULL) {
            systemPrintDebug("Attempted to access invalid SRAM bank: %d\n", this->sramBank);
        }

        this->gameboy->mmu->mapBank(0xA, NULL);
        this->gameboy->mmu->mapBank(0xB, NULL);
    }
}

void MBC::mapBanks() {
    this->mapRomBank0();
    this->mapRomBank1();
    this->mapSramBank();

    if(this->readFunc != NULL) {
        auto read = [this](u16 addr) -> u8 {
            return (this->*readFunc)(addr);
        };

        this->gameboy->mmu->mapBankReadFunc(0xA, read);
        this->gameboy->mmu->mapBankReadFunc(0xB, read);
    }

    if(this->writeFunc != NULL) {
        auto write = [this](u16 addr, u8 val) -> void {
            (this->*writeFunc)(addr, val);
        };

        this->gameboy->mmu->mapBankWriteFunc(0x0, write);
        this->gameboy->mmu->mapBankWriteFunc(0x1, write);
        this->gameboy->mmu->mapBankWriteFunc(0x2, write);
        this->gameboy->mmu->mapBankWriteFunc(0x3, write);
        this->gameboy->mmu->mapBankWriteFunc(0x4, write);
        this->gameboy->mmu->mapBankWriteFunc(0x5, write);
        this->gameboy->mmu->mapBankWriteFunc(0x6, write);
        this->gameboy->mmu->mapBankWriteFunc(0x7, write);
        this->gameboy->mmu->mapBankWriteFunc(0xA, write);
        this->gameboy->mmu->mapBankWriteFunc(0xB, write);
    }
}

/* MBC read handlers */

/* MBC3 */
u8 MBC::m3r(u16 addr) {
    if(this->sramEnabled) {
        switch(this->sramBank) { // Check for RTC register
            case 0x8:
                return (u8) this->gbClock.mbc3.s;
            case 0x9:
                return (u8) this->gbClock.mbc3.m;
            case 0xA:
                return (u8) this->gbClock.mbc3.h;
            case 0xB:
                return (u8) (this->gbClock.mbc3.d & 0xFF);
            case 0xC:
                return (u8) this->gbClock.mbc3.ctrl;
            default: // Not an RTC register
                return this->readSram((u16) (addr & 0x1FFF));
        }
    }

    return 0xFF;
}

/* MBC7 */
u8 MBC::m7r(u16 addr) {
    switch(addr & 0xF0) {
        case 0x00:
        case 0x10:
        case 0x60:
        case 0x70:
            return 0;
        case 0x20:
            return (u8) (inputGetMotionSensorX() & 0xFF);
        case 0x30:
            return (u8) ((inputGetMotionSensorX() >> 8) & 0xFF);
        case 0x40:
            return (u8) (inputGetMotionSensorY() & 0xFF);
        case 0x50:
            return (u8) ((inputGetMotionSensorY() >> 8) & 0xFF);
        case 0x80:
            return this->mbc7RA;
        default:
            return 0xFF;
    }
}

/* HUC3 */
u8 MBC::h3r(u16 addr) {
    switch(this->huc3Mode) {
        case 0xA:
            return this->readSram((u16) (addr & 0x1FFF));
        case 0xC:
            return this->huc3Value;
        case 0xB:
        case 0xD:
            /* Return 1 as a fixed value, needed for some games to
             * boot, the meaning is unknown. */
            return 1;
        default:
            break;
    }

    return 0xFF;
}

/* CAMERA */
u8 MBC::camr(u16 addr) {
    if(this->cameraIO) {
        u8 reg = (u8) (addr & 0x7F);
        if(reg == 0) {
            return this->cameraRegs[reg];
        }

        return 0;
    } else {
        return this->readSram((u16) (addr & 0x1FFF));
    }
}

/* MBC Write handlers */

/* MBC0 (ROM) */
void MBC::m0w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xa: /* A000 - BFFF */
        case 0xb:
            this->writeSram((u16) (addr & 0x1FFF), val);
            break;
        default:
            break;
    }
}

/* MBC1 */
void MBC::m1w(u16 addr, u8 val) {
    int newBank;

    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            this->sramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            val &= 0x1F;
            if(this->mbc1RockmanMapper) {
                newBank = ((val > 0xF) ? val - 8 : val);
            } else {
                newBank = (this->romBank1 & 0xE0) | val;
            }

            this->romBank1 = newBank ? newBank : 1;
            this->mapRomBank1();
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            val &= 3;
            if(this->mbc1RamMode) { /* RAM mode */
                this->sramBank = val;
                this->mapSramBank();
            } else { /* ROM mode */
                newBank = (this->romBank1 & 0x1F) | (val << 5);
                this->romBank1 = newBank ? newBank : 1;
                this->mapRomBank1();
            }

            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            this->mbc1RamMode = (bool) (val & 1);
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->sramEnabled) {
                this->writeSram((u16) (addr & 0x1FFF), val);
            }

            break;
        default:
            break;
    }
}

/* MBC2 */
void MBC::m2w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            this->sramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            this->romBank1 = val ? val : 1;
            this->mapRomBank1();
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->sramEnabled) {
                this->writeSram((u16) (addr & 0x1FFF), (u8) (val & 0xF));
            }

            break;
        default:
            break;
    }
}

/* MBC3 */
void MBC::m3w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            this->sramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            val &= 0x7F;
            this->romBank1 = val ? val : 1;
            this->mapRomBank1();
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            /* The RTC register is selected by writing values 0x8-0xc, ram banks
             * are selected by values 0x0-0x3 */
            this->sramBank = val;
            this->mapSramBank();
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            if(val) {
                this->latchClock();
            }

            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(!this->sramEnabled) {
                break;
            }

            switch(this->sramBank) { // Check for RTC register
                case 0x8:
                    if(this->gbClock.mbc3.s != val) {
                        this->gbClock.mbc3.s = val;
                    }

                    return;
                case 0x9:
                    if(this->gbClock.mbc3.m != val) {
                        this->gbClock.mbc3.m = val;
                    }

                    return;
                case 0xA:
                    if(this->gbClock.mbc3.h != val) {
                        this->gbClock.mbc3.h = val;
                    }

                    return;
                case 0xB:
                    if((this->gbClock.mbc3.d & 0xff) != val) {
                        this->gbClock.mbc3.d &= 0x100;
                        this->gbClock.mbc3.d |= val;
                    }

                    return;
                case 0xC:
                    if(this->gbClock.mbc3.ctrl != val) {
                        this->gbClock.mbc3.d &= 0xFF;
                        this->gbClock.mbc3.d |= (val & 1) << 8;
                        this->gbClock.mbc3.ctrl = val;
                    }

                    return;
                default: // Not an RTC register
                    this->writeSram((u16) (addr & 0x1FFF), val);
            }

            break;
        default:
            break;
    }
}

/* MBC5 */
void MBC::m5w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            this->sramEnabled = ((val & 0xF) == 0xA);
            break;
        case 0x2: /* 2000 - 3FFF */
            this->romBank1 = (this->romBank1 & 0x100) | val;
            this->mapRomBank1();
            break;
        case 0x3:
            this->romBank1 = (this->romBank1 & 0xFF) | (val & 1) << 8;
            this->mapRomBank1();
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            /* MBC5 might have a rumble motor, which is triggered by the
             * 4th bit of the value written */
            if(this->gameboy->romFile->hasRumble()) {
                systemSetRumble((val & 0x8) != 0);
                val &= 0x7;
            } else {
                val &= 0xF;
            }

            this->sramBank = val;
            this->mapSramBank();
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->sramEnabled) {
                this->writeSram((u16) (addr & 0x1FFF), val);
            }

            break;
        default:
            break;
    }
}

/* MBC6 */
void MBC::m6w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            this->sramEnabled = ((val & 0xF) == 0xA);
            break;
        case 0x2: /* 2000 - 3FFF */
            if(!(addr & 0x0800)) {
                this->romBank1ALatch = val & 0x7F;
            } else if(val == 0) {
                this->romBank1A = this->romBank1ALatch;
                this->mapRomBank1();
            }

            break;
        case 0x3:
            if(!(addr & 0x0800)) {
                this->romBank1BLatch = val & 0x7F;
            } else if(val == 0) {
                this->romBank1B = this->romBank1BLatch;
                this->mapRomBank1();
            }

            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->sramEnabled) {
                this->writeSram((u16) (addr & 0x1FFF), val);
            }

            break;
        default:
            break;
    }
}

#define MBC7_STATE_NONE 0
#define MBC7_STATE_IDLE 1
#define MBC7_STATE_READ_COMMAND 2
#define MBC7_STATE_READ_ADDRESS 3
#define MBC7_STATE_EXECUTE_COMMAND 4
#define MBC7_STATE_READ 5
#define MBC7_STATE_WRITE 6

#define MBC7_COMMAND_0 0
#define MBC7_COMMAND_WRITE 1
#define MBC7_COMMAND_READ 2
#define MBC7_COMMAND_FILL 3

/* MBC7 */
void MBC::m7w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            val &= 0x7F;
            this->romBank1 = val ? val : 1;
            this->mapRomBank1();
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            this->sramBank = val & 0xF;
            this->mapSramBank();
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if((addr & 0xF0) == 0x80) {
                int oldCs = this->mbc7Cs;
                int oldSk = this->mbc7Sk;

                this->mbc7Cs = val >> 7;
                this->mbc7Sk = (u8) ((val >> 6) & 1);

                if(!oldCs && this->mbc7Cs) {
                    if(this->mbc7State == MBC7_STATE_WRITE) {
                        if(this->mbc7WriteEnable) {
                            this->writeSram((u16) (this->mbc7Addr * 2), (u8) (this->mbc7Buffer >> 8));
                            this->writeSram((u16) (this->mbc7Addr * 2 + 1), (u8) (this->mbc7Buffer & 0xff));
                        }

                        this->mbc7State = MBC7_STATE_NONE;
                        this->mbc7RA = 1;
                    } else {
                        this->mbc7State = MBC7_STATE_IDLE;
                    }
                }

                if(!oldSk && this->mbc7Sk) {
                    if(this->mbc7State > MBC7_STATE_IDLE && this->mbc7State < MBC7_STATE_READ) {
                        this->mbc7Buffer <<= 1;
                        this->mbc7Buffer |= (val & 0x2) ? 1 : 0;
                        this->mbc7Count++;
                    }

                    switch(this->mbc7State) {
                        case MBC7_STATE_NONE:
                            break;
                        case MBC7_STATE_IDLE:
                            if(val & 0x02) {
                                this->mbc7Count = 0;
                                this->mbc7Buffer = 0;
                                this->mbc7State = MBC7_STATE_READ_COMMAND;
                            }

                            break;
                        case MBC7_STATE_READ_COMMAND:
                            if(this->mbc7Count == 2) {
                                this->mbc7State = MBC7_STATE_READ_ADDRESS;
                                this->mbc7Count = 0;
                                this->mbc7OpCode = (u8) (this->mbc7Buffer & 3);
                            }

                            break;
                        case MBC7_STATE_READ_ADDRESS:
                            if(this->mbc7Count == 8) {
                                this->mbc7State = MBC7_STATE_EXECUTE_COMMAND;
                                this->mbc7Count = 0;
                                this->mbc7Addr = (u8) (this->mbc7Buffer & 0xFF);
                                if(this->mbc7OpCode == 0) {
                                    switch(this->mbc7Addr >> 6) {
                                        case 0:
                                            this->mbc7WriteEnable = false;
                                            this->mbc7State = MBC7_STATE_NONE;
                                            break;
                                        case 3:
                                            this->mbc7WriteEnable = true;
                                            this->mbc7State = MBC7_STATE_NONE;
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }

                            break;
                        case MBC7_STATE_EXECUTE_COMMAND:
                            switch(this->mbc7OpCode) {
                                case MBC7_COMMAND_0:
                                    if(this->mbc7Count == 16) {
                                        switch(this->mbc7Addr >> 6) {
                                            case 0:
                                                this->mbc7WriteEnable = false;
                                                this->mbc7State = MBC7_STATE_NONE;
                                                break;
                                            case 1:
                                                if(this->mbc7WriteEnable) {
                                                    for(int i = 0; i < 256; i++) {
                                                        this->writeSram((u16) (i * 2), (u8) (this->mbc7Buffer >> 8));
                                                        this->writeSram((u16) (i * 2 + 1), (u8) (this->mbc7Buffer & 0xFF));
                                                    }
                                                }

                                                this->mbc7State = MBC7_STATE_WRITE;
                                                break;
                                            case 2:
                                                if(this->mbc7WriteEnable) {
                                                    for(int i = 0; i < 256; i++) {
                                                        this->writeSram((u16) (i * 2), 0xFF);
                                                        this->writeSram((u16) (i * 2 + 1), 0xFF);
                                                    }
                                                }

                                                this->mbc7State = MBC7_STATE_WRITE;
                                                break;
                                            case 3:
                                                this->mbc7WriteEnable = true;
                                                this->mbc7State = MBC7_STATE_NONE;
                                                break;
                                            default:
                                                break;
                                        }

                                        this->mbc7Count = 0;
                                    }

                                    break;
                                case MBC7_COMMAND_WRITE:
                                    if(this->mbc7Count == 16) {
                                        this->mbc7State = MBC7_STATE_WRITE;
                                        this->mbc7RA = 0;
                                        this->mbc7Count = 0;
                                    }

                                    break;
                                case MBC7_COMMAND_READ:
                                    if(this->mbc7Count == 1) {
                                        this->mbc7State = MBC7_STATE_READ;
                                        this->mbc7Count = 0;
                                        this->mbc7Buffer = (this->readSram((u16) (this->mbc7Addr * 2)) << 8) | this->readSram((u16) (this->mbc7Addr * 2 + 1));
                                    }

                                    break;
                                case MBC7_COMMAND_FILL:
                                    if(this->mbc7Count == 16) {
                                        this->mbc7State = MBC7_STATE_WRITE;
                                        this->mbc7RA = 0;
                                        this->mbc7Count = 0;
                                        this->mbc7Buffer = 0xFFFF;
                                    }

                                    break;
                                default:
                                    break;
                            }

                            break;
                        default:
                            break;
                    }
                } else if(oldSk && !this->mbc7Sk) {
                    if(this->mbc7State == MBC7_STATE_READ) {
                        this->mbc7RA = (u8) ((this->mbc7Buffer & 0x8000) ? 1 : 0);
                        this->mbc7Buffer <<= 1;
                        this->mbc7Count++;
                        if(this->mbc7Count == 16) {
                            this->mbc7Count = 0;
                            this->mbc7State = MBC7_STATE_NONE;
                        }
                    }
                }
            }

            break;
        default:
            break;
    }
}

/* MMM01 */
void MBC::mmm01w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            if(this->mmm01BankSelected) {
                this->sramEnabled = (val & 0xF) == 0xA;
            } else {
                this->mmm01BankSelected = true;

                this->romBank0 = this->mmm01RomBaseBank;
                this->mapRomBank0();

                this->romBank1 = this->mmm01RomBaseBank + 1;
                this->mapRomBank1();
            }

            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            if(this->mmm01BankSelected) {
                this->romBank1 = this->mmm01RomBaseBank + (val ? val : 1);
            this->mapRomBank1();
            } else {
                this->mmm01RomBaseBank = (u8) (((val & 0x3F) % this->gameboy->romFile->getRomBanks()) + 2);
            }

            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            if(this->mmm01BankSelected) {
                this->sramBank = val;
                this->mapSramBank();
            }

            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->mmm01BankSelected && this->sramEnabled) {
                this->writeSram((u16) (addr & 0x1FFF), val);
            }

            break;
        default:
            break;
    }
}

/* HUC1 */
void MBC::h1w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            this->sramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            this->romBank1 = val & 0x3F;
            this->mapRomBank1();
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            val &= 3;
            if(this->huc1RamMode) {
                this->sramBank = val;
                this->mapSramBank();
            } else {
                this->romBank1 = val;
                this->mapRomBank1();
            }

            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            this->huc1RamMode = (bool) (val & 1);
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->sramEnabled) {
                this->writeSram((u16) (addr & 0x1FFF), val);
            }

            break;
        default:
            break;
    }
}

/* HUC3 */
void MBC::h3w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            this->huc3Mode = val;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            this->romBank1 = val ? val : 1;
            this->mapRomBank1();
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            this->sramBank = val & 0xF;
            this->mapSramBank();
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* a000 - bFFF */
        case 0xB:
            switch(this->huc3Mode) {
                case 0xA:
                    this->writeSram((u16) (addr & 0x1FFF), val);
                    break;
                case 0xB:
                    switch(val & 0xF0) {
                        case 0x10: /* Read clock */
                            if(this->huc3Shift > 24) {
                                break;
                            }

                            switch(this->huc3Shift) {
                                case 0:
                                case 4:
                                case 8:     /* Minutes */
                                    this->huc3Value = (u8) ((this->gbClock.huc3.m >> this->huc3Shift) & 0xF);
                                    break;
                                case 12:
                                case 16:
                                case 20:  /* Days */
                                    this->huc3Value = (u8) ((this->gbClock.huc3.d >> (this->huc3Shift - 12)) & 0xF);
                                    break;
                                case 24:                    /* Year */
                                    this->huc3Value = (u8) (this->gbClock.huc3.y & 0xF);
                                    break;
                                default:
                                    break;
                            }

                            this->huc3Shift += 4;
                            break;
                        case 0x40:
                            switch(val & 0xF) {
                                case 0:
                                case 4:
                                case 7:
                                    this->huc3Shift = 0;
                                    break;
                                default:
                                    break;
                            }

                            this->latchClock();
                            break;
                        case 0x50:
                            break;
                        case 0x60:
                            this->huc3Value = 1;
                            break;
                        default:
                            systemPrintDebug("Unhandled HuC3 command 0x%02X.\n", val);
                            break;
                    }

                    break;
                case 0xC:
                case 0xD:
                case 0xE:
                    break;
                default:
                    break;
            }

            break;
        default:
            break;
    }
}

/* CAMERA */
void MBC::camw(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            this->sramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            this->romBank1 = val ? val : 1;
            this->mapRomBank1();
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            this->cameraIO = val == 0x10;
            if(!this->cameraIO) {
                this->sramBank = val & 0xF;
                this->mapSramBank();
            }

            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->cameraIO) {
                u8 reg = (u8) (addr & 0x7F);
                if(reg < 0x36) {
                    this->cameraRegs[reg] = val;
                    if(reg == 0) {
                        this->cameraRegs[reg] &= 0x7;

                        if(val & 0x1) {
                            this->camTakePicture();
                        }
                    }
                }
            } else if(this->sramEnabled) {
                this->writeSram((u16) (addr & 0x1FFF), val);
            }

            break;
        default:
            break;
    }
}

/* TAMA5 */
void MBC::t5w(u16 addr, u8 val) {
    if(addr <= 0xA001) {
        switch(addr & 1) {
            case 0: {
                val &= 0xF;
                this->tama5Commands[this->tama5CommandNumber] = val;
                this->writeSram(0, val);
                if((this->tama5CommandNumber & 0xE) == 0) {
                    this->romBank1 = this->tama5Commands[0] | (this->tama5Commands[1] << 4);
                    this->mapRomBank1();

                    this->tama5Commands[0x0F] = 0;
                } else if((this->tama5CommandNumber & 0xE) == 4) {
                    this->tama5Commands[0x0F] = 1;
                    if(this->tama5CommandNumber == 4) {
                        this->tama5Commands[0x05] = 0;
                    }
                } else if((this->tama5CommandNumber & 0xE) == 6) {
                    this->tama5RamByteSelect = (this->tama5Commands[7] << 4) | (this->tama5Commands[6] & 0x0F);
                    if(this->tama5Commands[0x0F] && this->tama5CommandNumber == 7) {
                        int data = (this->tama5Commands[0x04] & 0x0F) | (this->tama5Commands[0x05] << 4);
                        if(this->tama5RamByteSelect == 0x8) {
                            switch (data & 0xF) {
                                case 0x7:
                                    this->gbClock.tama5.d = (this->gbClock.tama5.d / 10) * 10 + (data >> 4);
                                    break;
                                case 0x8:
                                    this->gbClock.tama5.d = (this->gbClock.tama5.d % 10) + (data >> 4) * 10;
                                    break;
                                case 0x9:
                                    this->gbClock.tama5.mon = (this->gbClock.tama5.mon / 10) * 10 + (data >> 4);
                                    break;
                                case 0xa:
                                    this->gbClock.tama5.mon = (this->gbClock.tama5.mon % 10) + (data >> 4) * 10;
                                    break;
                                case 0xb:
                                    this->gbClock.tama5.y = (this->gbClock.tama5.y % 1000) + (data >> 4) * 1000;
                                    break;
                                case 0xc:
                                    gbClock.tama5.y = (gbClock.tama5.y % 100) + (gbClock.tama5.y / 1000) * 1000 + (data >> 4) * 100;
                                    break;
                                default:
                                    break;
                            }
                        } else if(this->tama5RamByteSelect == 0x18) {
                            this->latchClock();

                            int seconds = (this->gbClock.tama5.s / 10) * 16 + this->gbClock.tama5.s % 10;
                            int secondsL = (this->gbClock.tama5.s % 10);
                            int secondsH = (this->gbClock.tama5.s / 10);
                            int minutes = (this->gbClock.tama5.m / 10) * 16 + this->gbClock.tama5.m % 10;
                            int hours = (this->gbClock.tama5.h / 10) * 16 + this->gbClock.tama5.h % 10;
                            int daysL = this->gbClock.tama5.d % 10;
                            int daysH = this->gbClock.tama5.d / 10;
                            int monthsL = this->gbClock.tama5.mon % 10;
                            int monthsH = this->gbClock.tama5.mon / 10;
                            int years3 = (this->gbClock.tama5.y / 100) % 10;
                            int years4 = (this->gbClock.tama5.y / 1000);

                            switch(data & 0xF) {
                                case 0x0:
                                    this->tama5RAM[this->tama5RamByteSelect] = (u8) secondsL;
                                    break;
                                case 0x1:
                                    this->tama5RAM[this->tama5RamByteSelect] = (u8) secondsH;
                                    break;
                                case 0x7:
                                    this->tama5RAM[this->tama5RamByteSelect] = (u8) daysL;
                                    break;
                                case 0x8:
                                    this->tama5RAM[this->tama5RamByteSelect] = (u8) daysH;
                                    break;
                                case 0x9:
                                    this->tama5RAM[this->tama5RamByteSelect] = (u8) monthsL;
                                    break;
                                case 0xA:
                                    this->tama5RAM[this->tama5RamByteSelect] = (u8) monthsH;
                                    break;
                                case 0xB:
                                    this->tama5RAM[this->tama5RamByteSelect] = (u8) years4;
                                    break;
                                case 0xC:
                                    this->tama5RAM[this->tama5RamByteSelect] = (u8) years3;
                                    break;
                                default :
                                    break;
                            }

                            this->tama5RAM[0x54] = (u8) seconds;
                            this->tama5RAM[0x64] = (u8) minutes;
                            this->tama5RAM[0x74] = (u8) hours;
                            this->tama5RAM[0x84] = (u8) (daysH * 16 + daysL);
                            this->tama5RAM[0x94] = (u8) (monthsH * 16 + monthsL);

                            this->writeSram(0, 1);
                        } else if(this->tama5RamByteSelect == 0x28) {
                            if((data & 0xF) == 0xB) {
                                this->gbClock.tama5.y = ((this->gbClock.tama5.y >> 2) << 2) + (data & 3);
                            }
                        } else if(this->tama5RamByteSelect == 0x44) {
                            this->gbClock.tama5.m = (data / 16) * 10 + data % 16;
                        } else if(this->tama5RamByteSelect == 0x54) {
                            this->gbClock.tama5.h = (data / 16) * 10 + data % 16;
                        } else {
                            this->tama5RAM[this->tama5RamByteSelect] = (u8) data;
                        }
                    }
                }

                break;
            }
            case 1: {
                this->tama5CommandNumber = val;
                this->writeSram(1, val);
                if(val == 0x0A) {
                    for(int i = 0; i < 0x10; i++) {
                        for(int j = 0; j < 0x10; j++) {
                            if(!(j & 2)) {
                                this->tama5RAM[((i * 0x10) + j) | 2] = this->tama5RAM[(i * 0x10) + j];
                            }
                        }
                    }

                    this->sramEnabled = true;
                    this->writeSram(0, 1);
                } else if((val & 0x0E) == 0x0C) {
                    this->tama5RamByteSelect = this->tama5Commands[6] | (this->tama5Commands[7] << 4);

                    u8 byte = this->tama5RAM[this->tama5RamByteSelect];
                    this->writeSram(0, (u8) ((val & 1) ? byte >> 4 : byte & 0x0F));

                    this->tama5Commands[0x0F] = 0;
                }

                break;
            }
            default:
                break;
        }
    } else if(this->sramEnabled) {
        this->writeSram((u16) (addr & 0x1FFF), val);
    }
}

void MBC::camu() {
    if(this->cameraReadyCycle != 0 && this->gameboy->cpu->getCycle() >= this->cameraReadyCycle) {
        this->cameraRegs[0] &= ~0x1;
        this->cameraReadyCycle = 0;
    }
}

#define CAMERA_SENSOR_EXTRA_LINES 8
#define CAMERA_SENSOR_WIDTH 128
#define CAMERA_SENSOR_HEIGHT (112 + CAMERA_SENSOR_EXTRA_LINES)
#define CAMERA_EXPOSURE_REFERENCE 512

#define CAMERA_PROCESSED_WIDTH 128
#define CAMERA_PROCESSED_HEIGHT 112

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CLAMP(min, v, max) MIN(MAX((v), (min)), (max))

static const float edgeRatioLUT[8] = {0.50, 0.75, 1.00, 1.25, 2.00, 3.00, 4.00, 5.00};

void MBC::camTakePicture() {
    u32 pBits = (u32) (((this->cameraRegs[0] >> 1) & 3) != 0);
    u32 mBits = (u32) ((this->cameraRegs[0] >> 1) & 2);
    u32 nBit = (u32) ((this->cameraRegs[1] & 0x80) >> 7);
    u32 vhBits = (u32) ((this->cameraRegs[1] & 0x60) >> 5);
    u32 exposureBits = this->cameraRegs[3] | (this->cameraRegs[2] << 8);
    u32 iBit = (u32) ((this->cameraRegs[4] & 0x08) >> 3);
    float edgeAlpha = edgeRatioLUT[(this->cameraRegs[4] & 0x70) >> 4];
    u32 e3Bit = (u32) ((this->cameraRegs[4] & 0x80) >> 7);

    this->cameraReadyCycle = this->gameboy->cpu->getCycle() + exposureBits * 64 + (nBit ? 0 : 2048) + 129784;
    this->gameboy->cpu->setEventCycle(this->cameraReadyCycle);

    u32* image = systemGetCameraImage();
    if(image == NULL) {
        return;
    }

    int filtered[CAMERA_SENSOR_WIDTH * CAMERA_SENSOR_HEIGHT];

    for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
        u32* line = &image[y * CAMERA_SENSOR_WIDTH];
        for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
            int val = (int) CLAMP(0, ((((((2 * (line[x] & 0xFF) + 5 * ((line[x] >> 8) & 0xFF) + 1 * ((line[x] >> 16) & 0xFF)) >> 3) * exposureBits) / CAMERA_EXPOSURE_REFERENCE) - 128) / 8) + 128, 255);
            if(iBit) {
                val = 255 - val;
            }

            filtered[y * CAMERA_SENSOR_WIDTH + x] = val - 128;
        }
    }

    int tempBuf[CAMERA_SENSOR_WIDTH * CAMERA_SENSOR_HEIGHT];

    u32 filterMode = (nBit << 3) | (vhBits << 1) | e3Bit;
    switch(filterMode) {
        case 0x0:
            memcpy(tempBuf, filtered, sizeof(tempBuf));

            for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
                int* line = &filtered[y * CAMERA_SENSOR_WIDTH];
                int* tempLine = &tempBuf[y * CAMERA_SENSOR_WIDTH];
                int* tempLineS = &tempBuf[MIN(y + 1, CAMERA_SENSOR_HEIGHT - 1) * CAMERA_SENSOR_WIDTH];

                for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
                    int value = 0;
                    if(pBits & 0x1) value += tempLine[x];
                    if(pBits & 0x2) value += tempLineS[x];
                    if(mBits & 0x1) value -= tempLine[x];
                    if(mBits & 0x2) value -= tempLineS[x];
                    line[x] = CLAMP(-128, value, 127);
                }
            }

            break;
        case 0x1:
            memset(filtered, 0, sizeof(filtered));
            break;
        case 0x2:
            for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
                int* line = &filtered[y * CAMERA_SENSOR_WIDTH];
                int* tempLine = &tempBuf[y * CAMERA_SENSOR_WIDTH];

                for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
                    tempLine[x] = CLAMP(0, (int) (line[x] + ((2 * line[x] - line[MAX(0, x - 1)] - line[MIN(x + 1, CAMERA_SENSOR_WIDTH - 1)]) * edgeAlpha)), 255);
                }
            }

            for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
                int* line = &filtered[y * CAMERA_SENSOR_WIDTH];
                int* tempLine = &tempBuf[y * CAMERA_SENSOR_WIDTH];
                int* tempLineS = &tempBuf[MIN(y + 1, CAMERA_SENSOR_HEIGHT - 1) * CAMERA_SENSOR_WIDTH];

                for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
                    int value = 0;
                    if(pBits & 0x1) value += tempLine[x];
                    if(pBits & 0x2) value += tempLineS[x];
                    if(mBits & 0x1) value -= tempLine[x];
                    if(mBits & 0x2) value -= tempLineS[x];
                    line[x] = CLAMP(-128, value, 127);
                }
            }

            break;
        case 0xE:
            for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
                int* line = &filtered[y * CAMERA_SENSOR_WIDTH];
                int* lineS = &filtered[MIN(y + 1, CAMERA_SENSOR_HEIGHT - 1) * CAMERA_SENSOR_WIDTH];
                int* lineN = &filtered[MAX(0, y - 1) * CAMERA_SENSOR_WIDTH];
                int* tempLine = &tempBuf[y * CAMERA_SENSOR_WIDTH];

                for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
                    tempLine[x] = CLAMP(-128, (int) (line[x] + ((4 * line[x] - line[MAX(0, x - 1)] - line[MIN(x + 1, CAMERA_SENSOR_WIDTH - 1)] - lineN[x] - lineS[x]) * edgeAlpha)), 127);
                }
            }

            memcpy(filtered, tempBuf, sizeof(filtered));
            break;
        default:
            systemPrintDebug("Unsupported camera filter mode: 0x%x\n", filterMode);
            break;
    }

    int processed[CAMERA_PROCESSED_WIDTH * CAMERA_PROCESSED_HEIGHT];

    for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
        int* line = &filtered[(y + (CAMERA_SENSOR_EXTRA_LINES / 2)) * CAMERA_SENSOR_WIDTH];
        int* processedLine = &processed[y * CAMERA_SENSOR_WIDTH];

        for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
            int base = 6 + ((y & 3) * 4 + (x & 3)) * 3;
            u32 value = (u32) (line[x] + 128);
            if(value < this->cameraRegs[base + 0]) {
                processedLine[x] = 0x00;
            } else if(value < this->cameraRegs[base + 1]) {
                processedLine[x] = 0x40;
            } else if(value < this->cameraRegs[base + 2]) {
                processedLine[x] = 0x80;
            } else {
                processedLine[x] = 0xC0;
            }
        }
    }

    for(int y = 0; y < CAMERA_PROCESSED_HEIGHT; y++) {
        int* line = &processed[y * CAMERA_PROCESSED_WIDTH];

        for(int x = 0; x < CAMERA_PROCESSED_WIDTH; x++) {
            u8 color = (u8) (3 - (line[x] >> 6));
            u16 addr = (u16) (0x0100 + ((y >> 3) * 16 + (x >> 3)) * 16 + ((y & 7) * 2));

            if(color & 1) {
                this->writeSram(addr, (u8) (1 << (7 - (7 & x))));
            }

            if(color & 2) {
                this->writeSram((u16) (addr + 1), (u8) (1 << (7 - (7 & x))));
            }
        }
    }
}

#undef MIN
#undef MAX
#undef CLAMP

/* Increment y if x is greater than val */
#define OVERFLOW(x, val, y) ({ \
    while ((x) >= (val)) {     \
        (x) -= (val);          \
        (y)++;                 \
    }                          \
})

static int daysInMonth[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int daysInLeapMonth[12] = {
        31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

void MBC::latchClock() {
    time_t now;
    time(&now);

    time_t difference = now - this->gbClock.last;
    struct tm* lt = gmtime((const time_t*) &difference);

    switch(this->gameboy->romFile->getMBCType()) {
        case MBC3:
            this->gbClock.mbc3.s += lt->tm_sec;
            OVERFLOW(this->gbClock.mbc3.s, 60, this->gbClock.mbc3.m);
            this->gbClock.mbc3.m += lt->tm_min;
            OVERFLOW(this->gbClock.mbc3.m, 60, this->gbClock.mbc3.h);
            this->gbClock.mbc3.h += lt->tm_hour;
            OVERFLOW(this->gbClock.mbc3.h, 24, this->gbClock.mbc3.d);
            this->gbClock.mbc3.d += lt->tm_yday;
            /* Overflow! */
            if(this->gbClock.mbc3.d > 0x1FF) {
                /* Set the carry bit */
                this->gbClock.mbc3.ctrl |= 0x80;
                this->gbClock.mbc3.d &= 0x1FF;
            }

            /* The 9th bit of the day register is in the control register */
            this->gbClock.mbc3.ctrl &= ~1;
            this->gbClock.mbc3.ctrl |= (this->gbClock.mbc3.d > 0xff);
            break;
        case HUC3:
            this->gbClock.huc3.m += lt->tm_min;
            OVERFLOW(this->gbClock.huc3.m, 60 * 24, this->gbClock.huc3.d);
            this->gbClock.huc3.d += lt->tm_yday;
            OVERFLOW(this->gbClock.huc3.d, 365, this->gbClock.huc3.y);
            this->gbClock.huc3.y += lt->tm_year - 70;
            break;
        case TAMA5:
            this->gbClock.tama5.s += lt->tm_sec;
            OVERFLOW(this->gbClock.tama5.s, 60, this->gbClock.tama5.m);
            this->gbClock.tama5.m += lt->tm_min;
            OVERFLOW(this->gbClock.tama5.m, 60, this->gbClock.tama5.h);
            this->gbClock.tama5.h += lt->tm_hour;
            OVERFLOW(this->gbClock.tama5.h, 24, this->gbClock.tama5.d);
            this->gbClock.tama5.d += lt->tm_mday;
            OVERFLOW(this->gbClock.tama5.d, (this->gbClock.tama5.y & 3) == 0 ? daysInLeapMonth[this->gbClock.tama5.mon] : daysInMonth[this->gbClock.tama5.mon], this->gbClock.tama5.mon);
            this->gbClock.tama5.mon += lt->tm_mon;
            OVERFLOW(this->gbClock.tama5.mon, 12, this->gbClock.tama5.y);
            this->gbClock.tama5.y += lt->tm_year - 70;
            break;
        default:
            break;
    }

    this->gbClock.last = (u32) now;
}
