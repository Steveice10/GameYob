#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <platform/common/manager.h>
#include <platform/common/menu.h>

#include "platform/input.h"
#include "platform/system.h"
#include "gameboy.h"
#include "mbc.h"
#include "mmu.h"
#include "romfile.h"

MBC::MBC(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

MBC::~MBC() {
    if(this->banks != NULL) {
        for(int i = 0; i < this->ramBanks; i++) {
            if(this->banks[i] != NULL) {
                delete this->banks[i];
                this->banks[i] = NULL;
            }
        }

        delete this->banks;
        this->banks = NULL;
    }

    if(this->file != NULL) {
        fclose(this->file);
        this->file = NULL;
    }
}

void MBC::reset() {
    if(this->banks != NULL) {
        for(int i = 0; i < this->ramBanks; i++) {
            if(this->banks[i] != NULL) {
                delete this->banks[i];
                this->banks[i] = NULL;
            }
        }

        delete this->banks;
        this->banks = NULL;
    }

    if(this->file != NULL) {
        fclose(this->file);
        this->file = NULL;
    }

    memset(&this->gbClock, 0, sizeof(this->gbClock));

    this->ramBanks = this->gameboy->romFile->getRamBanks();
    if(this->ramBanks > 0) {
        this->file = fopen((this->gameboy->romFile->getFileName() + ".sav").c_str(), "w+b");
        if(this->file != NULL) {
            int neededFileSize = this->ramBanks * 0x2000;
            if(this->gameboy->romFile->getMBCType() == MBC3 || this->gameboy->romFile->getMBCType() == HUC3 || this->gameboy->romFile->getMBCType() == TAMA5) {
                neededFileSize += sizeof(this->gbClock);
            }

            struct stat s;
            fstat(fileno(this->file), &s);
            int fileSize = s.st_size;
            if(fileSize < neededFileSize) {
                fseek(this->file, neededFileSize - 1, SEEK_SET);
                fputc(0, this->file);
                fflush(this->file);
                fseek(this->file, 0, SEEK_SET);
            }

            this->banks = new u8*[this->ramBanks]();

            switch(this->gameboy->romFile->getMBCType()) {
                case MBC3:
                case HUC3:
                case TAMA5:
                    fread(&this->gbClock, 1, sizeof(this->gbClock), this->file);
                    break;
                default:
                    break;
            }
        } else {
            systemPrintDebug("Failed to open save file: %d\n", errno);
        }
    }

    this->mbcType = this->gameboy->romFile->getMBCType();

    this->readFunc = this->mbcReads[this->mbcType];
    this->writeFunc = this->mbcWrites[this->mbcType];

    // Rockman8 by Yang Yang uses a silghtly different MBC1 variant
    this->rockmanMapper = this->gameboy->romFile->getRomTitle().compare("ROCKMAN 99") == 0;

    memset(this->dirtyBanks, 0, sizeof(this->dirtyBanks));
    this->dirtyClock = false;

    // General
    this->romBank0Num = 0;
    this->romBank1Num = 1;
    this->ramBankNum = 0;
    this->ramEnabled = false;
    this->memoryModel = 0;

    // HuC3
    this->HuC3Mode = 0;
    this->HuC3Value = 0;
    this->HuC3Shift = 0;

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

    // MMM01
    this->mmm01BankSelected = false;
    this->mmm01RomBaseBank = 0;

    // CAMERA
    this->cameraIO = false;

    // TAMA5
    this->tama5CommandNumber = 0;
    this->tama5RamByteSelect = 0;
    memset(this->tama5Commands, 0, sizeof(this->tama5Commands));
    memset(this->tama5RAM, 0, sizeof(this->tama5RAM));

    this->mapBanks();
}

void MBC::save() {
    if(this->file == NULL) {
        return;
    }

    u32 banksSaved = 0;
    for(int bank = 0; bank < this->ramBanks; bank++) {
        if(this->dirtyBanks[bank]) {
            this->dirtyBanks[bank] = false;

            u8* bankPtr = this->getRamBank(bank);
            if(bankPtr != NULL) {
                fseek(this->file, bank * 0x2000, SEEK_SET);
                fwrite(bankPtr, 1, 0x2000, this->file);

                banksSaved++;
            }
        }
    }

    if(this->dirtyClock) {
        this->dirtyClock = false;

        fseek(this->file, this->ramBanks * 0x2000, SEEK_SET);
        switch(this->mbcType) {
            case MBC3:
            case HUC3:
            case TAMA5:
                fwrite(&this->gbClock, 1, sizeof(this->gbClock), this->file);
                break;
            default:
                break;
        }
    }
}

void MBC::loadState(FILE* file, int version) {
    fread(&this->ramBanks, 1, sizeof(this->ramBanks), file);

    long base = ftell(file);

    for(int bank = 0; bank < this->ramBanks; bank++) {
        u8* bankPtr = this->getRamBank(bank);
        if(bankPtr != NULL) {
            fseek(file, base + bank * 0x2000, SEEK_SET);
            fread(bankPtr, 1, 0x2000, file);
        }
    }

    fseek(file, base + this->ramBanks * 0x2000, SEEK_SET);
    fread(&this->gbClock, 1, sizeof(this->gbClock), file);

    fread(&this->mbcType, 1, sizeof(this->mbcType), file);
    fread(&this->rockmanMapper, 1, sizeof(this->rockmanMapper), file);
    fread(&this->romBank0Num, 1, sizeof(this->romBank0Num), file);
    fread(&this->romBank1Num, 1, sizeof(this->romBank1Num), file);
    fread(&this->ramBankNum, 1, sizeof(this->ramBankNum), file);
    fread(&this->ramEnabled, 1, sizeof(this->ramEnabled), file);
    fread(&this->memoryModel, 1, sizeof(this->memoryModel), file);

    switch(this->mbcType) {
        case HUC3:
            fread(&this->HuC3Mode, 1, sizeof(this->HuC3Mode), file);
            fread(&this->HuC3Value, 1, sizeof(this->HuC3Value), file);
            fread(&this->HuC3Shift, 1, sizeof(this->HuC3Shift), file);
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
        case MMM01:
            fread(&this->mmm01BankSelected, 1, sizeof(this->mmm01BankSelected), file);
            fread(&this->mmm01RomBaseBank, 1, sizeof(this->mmm01RomBaseBank), file);
            break;
        case CAMERA:
            fread(&this->cameraIO, 1, sizeof(this->cameraIO), file);
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
    fwrite(&this->ramBanks, 1, sizeof(this->ramBanks), file);

    long base = ftell(file);

    for(int bank = 0; bank < this->ramBanks; bank++) {
        u8* bankPtr = this->getRamBank(bank);
        if(bankPtr != NULL) {
            fseek(file, base + bank * 0x2000, SEEK_SET);
            fwrite(bankPtr, 1, 0x2000, file);
        }
    }

    fseek(file, base + this->ramBanks * 0x2000, SEEK_SET);
    fwrite(&this->gbClock, 1, sizeof(this->gbClock), file);

    fwrite(&this->mbcType, 1, sizeof(this->mbcType), file);
    fwrite(&this->rockmanMapper, 1, sizeof(this->rockmanMapper), file);
    fwrite(&this->romBank0Num, 1, sizeof(this->romBank0Num), file);
    fwrite(&this->romBank1Num, 1, sizeof(this->romBank1Num), file);
    fwrite(&this->ramBankNum, 1, sizeof(this->ramBankNum), file);
    fwrite(&this->ramEnabled, 1, sizeof(this->ramEnabled), file);
    fwrite(&this->memoryModel, 1, sizeof(this->memoryModel), file);

    switch(this->mbcType) {
        case HUC3:
            fwrite(&this->HuC3Mode, 1, sizeof(this->HuC3Mode), file);
            fwrite(&this->HuC3Value, 1, sizeof(this->HuC3Value), file);
            fwrite(&this->HuC3Shift, 1, sizeof(this->HuC3Shift), file);
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
        case MMM01:
            fwrite(&this->mmm01BankSelected, 1, sizeof(this->mmm01BankSelected), file);
            fwrite(&this->mmm01RomBaseBank, 1, sizeof(this->mmm01RomBaseBank), file);
            break;
        case CAMERA:
            fwrite(&this->cameraIO, 1, sizeof(this->cameraIO), file);
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

u8* MBC::getRamBank(int bank) {
    if(this->banks == NULL || bank < 0 || bank >= this->ramBanks) {
        return NULL;
    }

    if(this->banks[bank] == NULL) {
        this->banks[bank] = new u8[0x2000]();
        if(this->file != NULL) {
            fseek(this->file, bank * 0x2000, SEEK_SET);
            fread(this->banks[bank], 1, 0x2000, this->file);
        }
    }

    return this->banks[bank];
}

u8 MBC::readSram(u16 addr) {
    u8* bank = this->getRamBank(this->ramBankNum);
    if(bank != NULL) {
        return bank[addr & 0x1FFF];
    }

    return 0xFF;
}

void MBC::writeSram(u16 addr, u8 val) {
    u8* bank = this->getRamBank(this->ramBankNum);
    if(bank != NULL && bank[addr] != val) {
        bank[addr] = val;
        this->dirtyBanks[this->ramBankNum] = true;
    }
}

void MBC::mapRomBank0(int bank) {
    this->romBank0Num = bank;

    u8* bankPtr = this->gameboy->romFile->getRomBank(bank);
    if(bankPtr != NULL) {
        this->gameboy->mmu->mapBlock(0x0, bankPtr + 0x0000);
        this->gameboy->mmu->mapBlock(0x1, bankPtr + 0x1000);
        this->gameboy->mmu->mapBlock(0x2, bankPtr + 0x2000);
        this->gameboy->mmu->mapBlock(0x3, bankPtr + 0x3000);
    } else {
        systemPrintDebug("Attempted to access invalid ROM bank: %d\n", bank);

        this->gameboy->mmu->mapBlock(0x0, NULL);
        this->gameboy->mmu->mapBlock(0x1, NULL);
        this->gameboy->mmu->mapBlock(0x2, NULL);
        this->gameboy->mmu->mapBlock(0x3, NULL);
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

        this->gameboy->mmu->mapBlock(0x0, bios);
    }
}

void MBC::mapRomBank1(int bank) {
    this->romBank1Num = bank;

    u8* bankPtr = this->gameboy->romFile->getRomBank(bank);
    if(bankPtr != NULL) {
        this->gameboy->mmu->mapBlock(0x4, bankPtr + 0x0000);
        this->gameboy->mmu->mapBlock(0x5, bankPtr + 0x1000);
        this->gameboy->mmu->mapBlock(0x6, bankPtr + 0x2000);
        this->gameboy->mmu->mapBlock(0x7, bankPtr + 0x3000);
    } else {
        systemPrintDebug("Attempted to access invalid ROM bank: %d\n", bank);

        this->gameboy->mmu->mapBlock(0x4, NULL);
        this->gameboy->mmu->mapBlock(0x5, NULL);
        this->gameboy->mmu->mapBlock(0x6, NULL);
        this->gameboy->mmu->mapBlock(0x7, NULL);
    }
}

void MBC::mapRamBank(int bank) {
    this->ramBankNum = bank;

    u8* bankPtr = this->getRamBank(bank);
    if(bankPtr != NULL) {
        this->gameboy->mmu->mapBlock(0xA, bankPtr + 0x0000);
        this->gameboy->mmu->mapBlock(0xB, bankPtr + 0x1000);
    } else {
        // Only report if there's no chance it's a special bank number.
        if(this->readFunc == NULL) {
            systemPrintDebug("Attempted to access invalid RAM bank: %d\n", bank);
        }

        this->gameboy->mmu->mapBlock(0xA, NULL);
        this->gameboy->mmu->mapBlock(0xB, NULL);
    }
}

void MBC::mapBanks() {
    this->mapRomBank0(this->romBank0Num);
    this->mapRomBank1(this->romBank1Num);
    if(this->ramBanks > 0) {
        this->mapRamBank(this->ramBankNum);
    }

    if(this->readFunc != NULL) {
        this->gameboy->mmu->mapReadFunc(0xA, this, MBC::readEntry);
        this->gameboy->mmu->mapReadFunc(0xB, this, MBC::readEntry);
    }

    if(this->writeFunc != NULL) {
        this->gameboy->mmu->mapWriteFunc(0x0, this, MBC::writeEntry);
        this->gameboy->mmu->mapWriteFunc(0x1, this, MBC::writeEntry);
        this->gameboy->mmu->mapWriteFunc(0x2, this, MBC::writeEntry);
        this->gameboy->mmu->mapWriteFunc(0x3, this, MBC::writeEntry);
        this->gameboy->mmu->mapWriteFunc(0x4, this, MBC::writeEntry);
        this->gameboy->mmu->mapWriteFunc(0x5, this, MBC::writeEntry);
        this->gameboy->mmu->mapWriteFunc(0x6, this, MBC::writeEntry);
        this->gameboy->mmu->mapWriteFunc(0x7, this, MBC::writeEntry);
        this->gameboy->mmu->mapWriteFunc(0xA, this, MBC::writeEntry);
        this->gameboy->mmu->mapWriteFunc(0xB, this, MBC::writeEntry);
    }
}

u8 MBC::read(u16 addr) {
    return (this->*readFunc)(addr);
}

void MBC::write(u16 addr, u8 val) {
    (this->*writeFunc)(addr, val);
}

u8 MBC::readEntry(void* data, u16 addr) {
    return ((MBC*) data)->read(addr);
}

void MBC::writeEntry(void* data, u16 addr, u8 val) {
    ((MBC*) data)->write(addr, val);
}

/* MBC read handlers */

/* MBC3 */
u8 MBC::m3r(u16 addr) {
    if(!this->ramEnabled) {
        return 0xFF;
    }

    switch(this->ramBankNum) { // Check for RTC register
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

/* MBC7 */
u8 MBC::m7r(u16 addr) {
    switch(addr & 0xA0F0) {
        case 0xA000:
        case 0xA010:
        case 0xA060:
        case 0xA070:
            return 0;
        case 0xA020:
            return (u8) (inputGetMotionSensorX() & 0xFF);
        case 0xA030:
            return (u8) ((inputGetMotionSensorX() >> 8) & 0xFF);
        case 0xA040:
            return (u8) (inputGetMotionSensorX() & 0xFF);
        case 0xA050:
            return (u8) ((inputGetMotionSensorX() >> 8) & 0xFF);
        case 0xA080:
            return this->mbc7RA;
        default:
            return 0xFF;
    }
}

/* HUC3 */
u8 MBC::h3r(u16 addr) {
    switch(this->HuC3Mode) {
        case 0xC:
            return this->HuC3Value;
        case 0xB:
        case 0xD:
            /* Return 1 as a fixed value, needed for some games to
             * boot, the meaning is unknown. */
            return 1;
        default:
            return this->ramEnabled ? this->readSram((u16) (addr & 0x1FFF)) : (u8) 0xFF;
    }
}

/* CAMERA */
u8 MBC::camr(u16 addr) {
    if(this->cameraIO) {
        // 0xA000: hardware ready, Others: write only
        return (u8) (addr == 0xA000 ? 0x00 : 0xFF);
    } else {
        return this->ramEnabled ? this->readSram((u16) (addr & 0x1FFF)) : (u8) 0xFF;
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
            this->ramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            val &= 0x1F;
            if(this->rockmanMapper) {
                newBank = ((val > 0xF) ? val - 8 : val);
            } else {
                newBank = (this->romBank1Num & 0xE0) | val;
            }

            this->mapRomBank1(newBank ? newBank : 1);
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            val &= 3;
            if(this->memoryModel == 0) { /* ROM mode */
                newBank = (this->romBank1Num & 0x1F) | (val << 5);
                this->mapRomBank1(newBank ? newBank : 1);
            } else { /* RAM mode */
                this->mapRamBank(val);
            }

            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            this->memoryModel = val & 1;
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->ramEnabled) {
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
            this->ramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            this->mapRomBank1(val ? val : 1);
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->ramEnabled) {
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
            this->ramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            val &= 0x7F;
            this->mapRomBank1(val ? val : 1);
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            /* The RTC register is selected by writing values 0x8-0xc, ram banks
             * are selected by values 0x0-0x3 */
            this->mapRamBank(val);
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            if(val) {
                this->latchClock();
            }

            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(!this->ramEnabled) {
                break;
            }

            switch(this->ramBankNum) { // Check for RTC register
                case 0x8:
                    if(this->gbClock.mbc3.s != val) {
                        this->gbClock.mbc3.s = val;
                        this->dirtyClock = true;
                    }

                    return;
                case 0x9:
                    if(this->gbClock.mbc3.m != val) {
                        this->gbClock.mbc3.m = val;
                        this->dirtyClock = true;
                    }

                    return;
                case 0xA:
                    if(this->gbClock.mbc3.h != val) {
                        this->gbClock.mbc3.h = val;
                        this->dirtyClock = true;
                    }

                    return;
                case 0xB:
                    if((this->gbClock.mbc3.d & 0xff) != val) {
                        this->gbClock.mbc3.d &= 0x100;
                        this->gbClock.mbc3.d |= val;
                        this->dirtyClock = true;
                    }

                    return;
                case 0xC:
                    if(this->gbClock.mbc3.ctrl != val) {
                        this->gbClock.mbc3.d &= 0xFF;
                        this->gbClock.mbc3.d |= (val & 1) << 8;
                        this->gbClock.mbc3.ctrl = val;
                        this->dirtyClock = true;
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
            this->ramEnabled = ((val & 0xF) == 0xA);
            break;
        case 0x2: /* 2000 - 3FFF */
            this->mapRomBank1((this->romBank1Num & 0x100) | val);
            break;
        case 0x3:
            this->mapRomBank1((this->romBank1Num & 0xFF) | (val & 1) << 8);
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            /* MBC5 might have a rumble motor, which is triggered by the
             * 4th bit of the value written */
            if(this->gameboy->romFile->hasRumble()) {
                val &= 0x07;
            } else {
                val &= 0x0f;
            }

            this->mapRamBank(val);
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->ramEnabled) {
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
            this->ramEnabled = ((val & 0xF) == 0xA);
            break;
        case 0x2: /* 2000 - 3FFF */
            this->mapRomBank1((this->romBank1Num & 0x100) | val);
            break;
        case 0x3:
            this->mapRomBank1((this->romBank1Num & 0xFF) | (val & 1) << 8);
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            this->mapRamBank(val & 0xF);
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(addr == 0xA080) {
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
                this->ramEnabled = (val & 0xF) == 0xA;
            } else {
                this->mmm01BankSelected = true;

                this->mapRomBank0(this->mmm01RomBaseBank);
                this->mapRomBank1(this->mmm01RomBaseBank + 1);
            }

            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            if(this->mmm01BankSelected) {
                this->mapRomBank1(this->mmm01RomBaseBank + (val ? val : 1));
            } else {
                this->mmm01RomBaseBank = (u8) (((val & 0x3F) % this->gameboy->romFile->getRomBanks()) + 2);
            }

            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            if(this->mmm01BankSelected) {
                this->mapRamBank(val);
            }

            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->mmm01BankSelected && this->ramEnabled) {
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
            this->ramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            this->mapRomBank1(val & 0x3F);
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            val &= 3;
            if(this->memoryModel == 0) {
                this->mapRomBank1(val);
            } else {
                this->mapRamBank(val);
            }

            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            this->memoryModel = val & 1;
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->ramEnabled) {
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
            this->ramEnabled = (val & 0xF) == 0xA;
            this->HuC3Mode = val;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            this->mapRomBank1(val ? val : 1);
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            this->mapRamBank(val & 0xF);
            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* a000 - bFFF */
        case 0xB:
            switch(this->HuC3Mode) {
                case 0xB:
                    switch(val & 0xF0) {
                        case 0x10: /* Read clock */
                            if(this->HuC3Shift > 24) {
                                break;
                            }

                            switch(this->HuC3Shift) {
                                case 0:
                                case 4:
                                case 8:     /* Minutes */
                                    this->HuC3Value = (u8) ((this->gbClock.huc3.m >> this->HuC3Shift) & 0xF);
                                    break;
                                case 12:
                                case 16:
                                case 20:  /* Days */
                                    this->HuC3Value = (u8) ((this->gbClock.huc3.d >> (this->HuC3Shift - 12)) & 0xF);
                                    break;
                                case 24:                    /* Year */
                                    this->HuC3Value = (u8) (this->gbClock.huc3.y & 0xF);
                                    break;
                                default:
                                    break;
                            }

                            this->HuC3Shift += 4;
                            break;
                        case 0x40:
                            switch(val & 0xF) {
                                case 0:
                                case 4:
                                case 7:
                                    this->HuC3Shift = 0;
                                    break;
                                default:
                                    break;
                            }

                            this->latchClock();
                            break;
                        case 0x50:
                            break;
                        case 0x60:
                            this->HuC3Value = 1;
                            break;
                        default:
                            systemPrintDebug("Unhandled HuC3 command 0x%02X.\n", val);
                    }

                    break;
                case 0xC:
                case 0xD:
                case 0xE:
                    break;
                default:
                    if(this->ramEnabled) {
                        this->writeSram((u16) (addr & 0x1FFF), val);
                    }
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
            this->ramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            this->mapRomBank1(val ? val : 1);
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            this->cameraIO = val == 0x10;
            if(!this->cameraIO) {
                this->mapRamBank(val & 0xF);
            }

            break;
        case 0x6: /* 6000 - 7FFF */
        case 0x7:
            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(this->cameraIO) {
                // TODO: Handle I/O registers?
            } else if(this->ramEnabled) {
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
                    this->mapRomBank1(this->tama5Commands[0] | (this->tama5Commands[1] << 4));
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

                            this->dirtyClock = true;
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
                                this->dirtyClock = true;
                            }
                        } else if(this->tama5RamByteSelect == 0x44) {
                            this->gbClock.tama5.m = (data / 16) * 10 + data % 16;
                            this->dirtyClock = true;
                        } else if(this->tama5RamByteSelect == 0x54) {
                            this->gbClock.tama5.h = (data / 16) * 10 + data % 16;
                            this->dirtyClock = true;
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

                    this->ramEnabled = true;
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
    } else if(this->ramEnabled) {
        this->writeSram((u16) (addr & 0x1FFF), val);
    }
}

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

    switch(this->mbcType) {
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

    this->dirtyClock = true;
}
