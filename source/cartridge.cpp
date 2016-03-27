#include <errno.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <string>

#include "platform/common/manager.h"
#include "platform/input.h"
#include "platform/system.h"
#include "cpu.h"
#include "gameboy.h"
#include "cartridge.h"
#include "mmu.h"

Cartridge::Cartridge(std::istream& romData, int romSize, std::istream& saveData, int saveSize) {
    // Round number of banks to next power of two.
    this->totalRomBanks = (romSize + 0x3FFF) / 0x4000;
    this->totalRomBanks--;
    this->totalRomBanks |= this->totalRomBanks >> 1;
    this->totalRomBanks |= this->totalRomBanks >> 2;
    this->totalRomBanks |= this->totalRomBanks >> 4;
    this->totalRomBanks |= this->totalRomBanks >> 8;
    this->totalRomBanks |= this->totalRomBanks >> 16;
    this->totalRomBanks++;

    if(this->totalRomBanks < 2) {
        this->totalRomBanks = 2;
    }

    size_t roundedSize = (size_t) (this->totalRomBanks * 0x4000);

    this->rom = new u8[roundedSize]();
    romData.read((char*) this->rom, romSize);

    // Most MMM01 dumps have the initial banks at the end of the ROM rather than the beginning, so check if this is the case and compensate.
    if(romSize > 0x8000) {
        // Check for the logo.
        static const u8 logo[] = {
                0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
                0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
                0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC ,0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
        };

        u8* checkBank = &this->rom[roundedSize - 0x8000];

        if(memcmp(logo, &checkBank[0x0104], sizeof(logo)) == 0) {
            // Check for MMM01.
            u8 mbcType = checkBank[0x0147];

            if(mbcType >= 0x0B && mbcType <= 0x0D) {
                u8* copy = new u8[roundedSize];
                memcpy(copy, this->rom, roundedSize);

                memcpy(this->rom, &copy[roundedSize - 0x8000], 0x8000);
                memcpy(&this->rom[0x8000], copy, roundedSize - 0x8000);

                delete copy;
            }
        }
    }

    this->romTitle = std::string(reinterpret_cast<char*>(&this->rom[0x0134]), this->rom[0x0143] == 0x80 || this->rom[0x0143] == 0xC0 ? 15 : 16);
    this->romTitle.erase(std::find_if(this->romTitle.rbegin(), this->romTitle.rend(), [](int c) { return c != 0; }).base(), this->romTitle.end());

    switch(this->getRawMBC()) {
        case 0x00:
        case 0x08:
        case 0x09:
            this->mbcType = MBC0;
            break;
        case 0x01:
        case 0x02:
        case 0x03:
            this->mbcType = MBC1;
            this->rockmanMapper = this->romTitle.compare("ROCKMAN 99") == 0;
            break;
        case 0x05:
        case 0x06:
            this->mbcType = MBC2;
            break;
        case 0x0B:
        case 0x0C:
        case 0x0D:
            this->mbcType = MMM01;
            break;
        case 0x0F:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            this->mbcType = MBC3;
            break;
        case 0x19:
        case 0x1A:
        case 0x1B:
            this->mbcType = MBC5;
            break;
        case 0x1C:
        case 0x1D:
        case 0x1E:
            this->mbcType = MBC5;
            this->rumble = true;
            break;
        case 0x20:
            this->mbcType = MBC6;
            break;
        case 0x22:
            this->mbcType = MBC7;
            break;
        case 0xEA: // Hack for SONIC5
            this->mbcType = MBC1;
            break;
        case 0xFC:
            this->mbcType = CAMERA;
            break;
        case 0xFD:
            this->mbcType = TAMA5;
            break;
        case 0xFE:
            this->mbcType = HUC3;
            break;
        case 0xFF:
            this->mbcType = HUC1;
            break;
        default:
            systemPrintDebug("Unsupported mapper value %02x\n", this->rom[0x0147]);
            this->mbcType = MBC5;
            break;
    }

    this->readFunc = this->mbcReads[this->mbcType];
    this->writeFunc = this->mbcWrites[this->mbcType];
    this->updateFunc = this->mbcUpdates[this->mbcType];

    if(this->mbcType == MBC2 || this->mbcType == MBC7 || this->mbcType == TAMA5) {
        this->totalRamBanks = 1;
    } else {
        switch(this->getRawRamSize()) {
            case 0:
                this->totalRamBanks = 0;
                break;
            case 1:
            case 2:
                this->totalRamBanks = 1;
                break;
            case 3:
                this->totalRamBanks = 4;
                break;
            case 4:
                this->totalRamBanks = 16;
                break;
            default:
                systemPrintDebug("Invalid RAM bank number: %x\nDefaulting to 4 banks.\n", this->getRawRamSize());
                this->totalRamBanks = 4;
                break;
        }
    }

    this->sram = new u8[this->totalRamBanks * 0x2000]();
    saveData.read((char*) this->sram, this->totalRamBanks * 0x2000);

    if(this->mbcType == MBC3 || this->mbcType == HUC3 || this->mbcType == TAMA5) {
        saveData.read((char*) &this->gbClock, sizeof(this->gbClock));
    } else {
        memset(&this->gbClock, 0, sizeof(this->gbClock));
    }
}

Cartridge::~Cartridge() {
    if(this->gameboy != NULL) {
        this->gameboy->mmu->mapBankBlock(0x0, NULL);
        this->gameboy->mmu->mapBankBlock(0x1, NULL);
        this->gameboy->mmu->mapBankBlock(0x2, NULL);
        this->gameboy->mmu->mapBankBlock(0x3, NULL);
        this->gameboy->mmu->mapBankBlock(0x4, NULL);
        this->gameboy->mmu->mapBankBlock(0x5, NULL);
        this->gameboy->mmu->mapBankBlock(0x6, NULL);
        this->gameboy->mmu->mapBankBlock(0x7, NULL);
        this->gameboy->mmu->mapBankBlock(0xA, NULL);
        this->gameboy->mmu->mapBankBlock(0xB, NULL);

        this->gameboy->mmu->mapBankReadFunc(0xA, NULL);
        this->gameboy->mmu->mapBankReadFunc(0xB, NULL);

        this->gameboy->mmu->mapBankWriteFunc(0x0, NULL);
        this->gameboy->mmu->mapBankWriteFunc(0x1, NULL);
        this->gameboy->mmu->mapBankWriteFunc(0x2, NULL);
        this->gameboy->mmu->mapBankWriteFunc(0x3, NULL);
        this->gameboy->mmu->mapBankWriteFunc(0x4, NULL);
        this->gameboy->mmu->mapBankWriteFunc(0x5, NULL);
        this->gameboy->mmu->mapBankWriteFunc(0x6, NULL);
        this->gameboy->mmu->mapBankWriteFunc(0x7, NULL);
        this->gameboy->mmu->mapBankWriteFunc(0xA, NULL);
        this->gameboy->mmu->mapBankWriteFunc(0xB, NULL);

        this->gameboy = NULL;
    }

    if(this->rom != NULL) {
        delete this->rom;
        this->rom = NULL;
    }

    if(this->sram != NULL) {
        delete this->sram;
        this->sram = NULL;
    }
}

void Cartridge::reset(Gameboy* gameboy) {
    this->gameboy = gameboy;

    // General
    this->romBank0 = 0;
    this->romBank1 = 1;
    this->sramBank = 0;
    this->sramEnabled = false;

    // MBC1
    this->mbc1RamMode = false;

    // MBC3
    this->mbc3Ctrl = 0;

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

void Cartridge::loadState(std::istream& data, u8 version) {
    data.read((char*) &this->romBank0, sizeof(this->romBank0));
    data.read((char*) &this->romBank1, sizeof(this->romBank1));
    data.read((char*) &this->sramBank, sizeof(this->sramBank));
    data.read((char*) &this->sramEnabled, sizeof(this->sramEnabled));

    switch(this->mbcType) {
        case MBC1:
            data.read((char*) &this->mbc1RamMode, sizeof(this->mbc1RamMode));
            break;
        case MBC3:
            data.read((char*) &this->mbc3Ctrl, sizeof(this->mbc3Ctrl));
            break;
        case MBC6:
            data.read((char*) &this->romBank1ALatch, sizeof(this->romBank1ALatch));
            data.read((char*) &this->romBank1BLatch, sizeof(this->romBank1BLatch));
            data.read((char*) &this->romBank1A, sizeof(this->romBank1A));
            data.read((char*) &this->romBank1B, sizeof(this->romBank1B));
            break;
        case MBC7:
            data.read((char*) &this->mbc7WriteEnable, sizeof(this->mbc7WriteEnable));
            data.read((char*) &this->mbc7Cs, sizeof(this->mbc7Cs));
            data.read((char*) &this->mbc7Sk, sizeof(this->mbc7Sk));
            data.read((char*) &this->mbc7OpCode, sizeof(this->mbc7OpCode));
            data.read((char*) &this->mbc7Addr, sizeof(this->mbc7Addr));
            data.read((char*) &this->mbc7Count, sizeof(this->mbc7Count));
            data.read((char*) &this->mbc7State, sizeof(this->mbc7State));
            data.read((char*) &this->mbc7Buffer, sizeof(this->mbc7Buffer));
            data.read((char*) &this->mbc7RA, sizeof(this->mbc7RA));
            break;
        case HUC1:
            data.read((char*) &this->huc1RamMode, sizeof(this->huc1RamMode));
            break;
        case HUC3:
            data.read((char*) &this->huc3Mode, sizeof(this->huc3Mode));
            data.read((char*) &this->huc3Value, sizeof(this->huc3Value));
            data.read((char*) &this->huc3Shift, sizeof(this->huc3Shift));
            break;
        case MMM01:
            data.read((char*) &this->mmm01BankSelected, sizeof(this->mmm01BankSelected));
            data.read((char*) &this->mmm01RomBaseBank, sizeof(this->mmm01RomBaseBank));
            break;
        case CAMERA:
            data.read((char*) &this->cameraIO, sizeof(this->cameraIO));
            data.read((char*) &this->cameraReadyCycle, sizeof(this->cameraReadyCycle));
            data.read((char*) this->cameraRegs, sizeof(this->cameraRegs));
            break;
        case TAMA5:
            data.read((char*) &this->tama5CommandNumber, sizeof(this->tama5CommandNumber));
            data.read((char*) &this->tama5RamByteSelect, sizeof(this->tama5RamByteSelect));
            data.read((char*) this->tama5Commands, sizeof(this->tama5Commands));
            data.read((char*) this->tama5RAM, sizeof(this->tama5RAM));
        default:
            break;
    }

    this->mapBanks();
}

void Cartridge::saveState(std::ostream& data) {
    data.write((char*) &this->romBank0, sizeof(this->romBank0));
    data.write((char*) &this->romBank1, sizeof(this->romBank1));
    data.write((char*) &this->sramBank, sizeof(this->sramBank));
    data.write((char*) &this->sramEnabled, sizeof(this->sramEnabled));

    switch(this->mbcType) {
        case MBC1:
            data.write((char*) &this->mbc1RamMode, sizeof(this->mbc1RamMode));
            break;
        case MBC3:
            data.write((char*) &this->mbc3Ctrl, sizeof(this->mbc3Ctrl));
            break;
        case MBC6:
            data.write((char*) &this->romBank1ALatch, sizeof(this->romBank1ALatch));
            data.write((char*) &this->romBank1BLatch, sizeof(this->romBank1BLatch));
            data.write((char*) &this->romBank1A, sizeof(this->romBank1A));
            data.write((char*) &this->romBank1B, sizeof(this->romBank1B));
            break;
        case MBC7:
            data.write((char*) &this->mbc7WriteEnable, sizeof(this->mbc7WriteEnable));
            data.write((char*) &this->mbc7Cs, sizeof(this->mbc7Cs));
            data.write((char*) &this->mbc7Sk, sizeof(this->mbc7Sk));
            data.write((char*) &this->mbc7OpCode, sizeof(this->mbc7OpCode));
            data.write((char*) &this->mbc7Addr, sizeof(this->mbc7Addr));
            data.write((char*) &this->mbc7Count, sizeof(this->mbc7Count));
            data.write((char*) &this->mbc7State, sizeof(this->mbc7State));
            data.write((char*) &this->mbc7Buffer, sizeof(this->mbc7Buffer));
            data.write((char*) &this->mbc7RA, sizeof(this->mbc7RA));
            break;
        case HUC1:
            data.write((char*) &this->huc1RamMode, sizeof(this->huc1RamMode));
            break;
        case HUC3:
            data.write((char*) &this->huc3Mode, sizeof(this->huc3Mode));
            data.write((char*) &this->huc3Value, sizeof(this->huc3Value));
            data.write((char*) &this->huc3Shift, sizeof(this->huc3Shift));
            break;
        case MMM01:
            data.write((char*) &this->mmm01BankSelected, sizeof(this->mmm01BankSelected));
            data.write((char*) &this->mmm01RomBaseBank, sizeof(this->mmm01RomBaseBank));
            break;
        case CAMERA:
            data.write((char*) &this->cameraIO, sizeof(this->cameraIO));
            data.write((char*) &this->cameraReadyCycle, sizeof(this->cameraReadyCycle));
            data.write((char*) this->cameraRegs, sizeof(this->cameraRegs));
            break;
        case TAMA5:
            data.write((char*) &this->tama5CommandNumber, sizeof(this->tama5CommandNumber));
            data.write((char*) &this->tama5RamByteSelect, sizeof(this->tama5RamByteSelect));
            data.write((char*) this->tama5Commands, sizeof(this->tama5Commands));
            data.write((char*) this->tama5RAM, sizeof(this->tama5RAM));
        default:
            break;
    }
}

void Cartridge::update() {
    if(this->updateFunc != NULL) {
        (this->*updateFunc)();
    }
}

void Cartridge::save(std::ostream& data) {
    data.write((char*) this->sram, this->totalRamBanks * 0x2000);

    if(this->mbcType == MBC3 || this->mbcType == HUC3 || this->mbcType == TAMA5) {
        data.write((char*) &this->gbClock, sizeof(this->gbClock));
    }
}

u8 Cartridge::readSram(u16 addr) {
    if(this->sramBank >= 0 && this->sramBank < this->totalRamBanks) {
        return this->sram[this->sramBank * 0x2000 + (addr & 0x1FFF)];
    } else {
        systemPrintDebug("Attempted to read from invalid SRAM bank: %d\n", this->sramBank);
        return 0xFF;
    }
}

void Cartridge::writeSram(u16 addr, u8 val) {
    if(this->sramBank >= 0 && this->sramBank < this->totalRamBanks) {
        this->sram[this->sramBank * 0x2000 + (addr & 0x1FFF)] = val;
    } else {
        systemPrintDebug("Attempted to write to invalid SRAM bank: %d\n", this->sramBank);
    }
}

void Cartridge::mapRomBank0() {
    u8* bankPtr = this->getRomBank(this->romBank0);
    if(bankPtr != NULL) {
        this->gameboy->mmu->mapBankBlock(0x0, bankPtr + 0x0000);
        this->gameboy->mmu->mapBankBlock(0x1, bankPtr + 0x1000);
        this->gameboy->mmu->mapBankBlock(0x2, bankPtr + 0x2000);
        this->gameboy->mmu->mapBankBlock(0x3, bankPtr + 0x3000);
    } else {
        systemPrintDebug("Attempted to access invalid ROM bank: %d\n", this->romBank0);

        this->gameboy->mmu->mapBankBlock(0x0, NULL);
        this->gameboy->mmu->mapBankBlock(0x1, NULL);
        this->gameboy->mmu->mapBankBlock(0x2, NULL);
        this->gameboy->mmu->mapBankBlock(0x3, NULL);
    }
}

void Cartridge::mapRomBank1() {
    if(this->mbcType == MBC6) {
        u8* bankPtr1A = this->getRomBank(this->romBank1A >> 1);
        if(bankPtr1A != NULL) {
            u8* subPtr = bankPtr1A + (this->romBank1A & 0x1) * 0x2000;

            this->gameboy->mmu->mapBankBlock(0x4, subPtr + 0x0000);
            this->gameboy->mmu->mapBankBlock(0x5, subPtr + 0x1000);
        } else {
            systemPrintDebug("Attempted to access invalid sub ROM bank: %d\n", this->romBank1A);

            this->gameboy->mmu->mapBankBlock(0x4, NULL);
            this->gameboy->mmu->mapBankBlock(0x5, NULL);
        }

        u8* bankPtr1B = this->getRomBank(this->romBank1B >> 1);
        if(bankPtr1B != NULL) {
            u8* subPtr = bankPtr1B + (this->romBank1B & 0x1) * 0x2000;

            this->gameboy->mmu->mapBankBlock(0x6, subPtr + 0x0000);
            this->gameboy->mmu->mapBankBlock(0x7, subPtr + 0x1000);
        } else {
            systemPrintDebug("Attempted to access invalid sub ROM bank: %d\n", this->romBank1B);

            this->gameboy->mmu->mapBankBlock(0x6, NULL);
            this->gameboy->mmu->mapBankBlock(0x7, NULL);
        }
    } else {
        u8* bankPtr = this->getRomBank(this->romBank1);
        if(bankPtr != NULL) {
            this->gameboy->mmu->mapBankBlock(0x4, bankPtr + 0x0000);
            this->gameboy->mmu->mapBankBlock(0x5, bankPtr + 0x1000);
            this->gameboy->mmu->mapBankBlock(0x6, bankPtr + 0x2000);
            this->gameboy->mmu->mapBankBlock(0x7, bankPtr + 0x3000);
        } else {
            systemPrintDebug("Attempted to access invalid ROM bank: %d\n", this->romBank1);

            this->gameboy->mmu->mapBankBlock(0x4, NULL);
            this->gameboy->mmu->mapBankBlock(0x5, NULL);
            this->gameboy->mmu->mapBankBlock(0x6, NULL);
            this->gameboy->mmu->mapBankBlock(0x7, NULL);
        }
    }
}

void Cartridge::mapSramBank() {
    if(this->sramBank >= 0 && this->sramBank < this->totalRamBanks) {
        u8* bankPtr = &this->sram[this->sramBank * 0x2000];

        this->gameboy->mmu->mapBankBlock(0xA, bankPtr + 0x0000);
        this->gameboy->mmu->mapBankBlock(0xB, bankPtr + 0x1000);
    } else {
        // Only report if there's no chance it's a special bank number.
        if(this->readFunc == NULL) {
            systemPrintDebug("Attempted to access invalid SRAM bank: %d\n", this->sramBank);
        }

        this->gameboy->mmu->mapBankBlock(0xA, NULL);
        this->gameboy->mmu->mapBankBlock(0xB, NULL);
    }
}

void Cartridge::mapBanks() {
    this->mapRomBank0();
    this->mapRomBank1();
    if(this->totalRamBanks > 0) {
        this->mapSramBank();
    }

    std::function<u8(u16 addr)> read = NULL;
    if(this->readFunc != NULL) {
        read = [this](u16 addr) -> u8 {
            return (this->*readFunc)(addr);
        };
    }

    this->gameboy->mmu->mapBankReadFunc(0xA, read);
    this->gameboy->mmu->mapBankReadFunc(0xB, read);

    std::function<void(u16 addr, u8 val)> write = NULL;
    if(this->writeFunc != NULL) {
        write = [this](u16 addr, u8 val) -> void {
            (this->*writeFunc)(addr, val);
        };
    }

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

/* MBC read handlers */

/* MBC3 */
u8 Cartridge::m3r(u16 addr) {
    if(this->sramEnabled) {
        switch(this->sramBank) { // Check for RTC register
            case 0x8:
                return (u8) this->gbClock.seconds;
            case 0x9:
                return (u8) this->gbClock.minutes;
            case 0xA:
                return (u8) this->gbClock.hours;
            case 0xB:
                return (u8) (this->gbClock.days & 0xFF);
            case 0xC:
                return this->mbc3Ctrl;
            default: // Not an RTC register
                return this->readSram((u16) (addr & 0x1FFF));
        }
    }

    return 0xFF;
}

/* MBC7 */
u8 Cartridge::m7r(u16 addr) {
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
u8 Cartridge::h3r(u16 addr) {
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
u8 Cartridge::camr(u16 addr) {
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
void Cartridge::m0w(u16 addr, u8 val) {
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
void Cartridge::m1w(u16 addr, u8 val) {
    int newBank;

    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            this->sramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2: /* 2000 - 3FFF */
        case 0x3:
            val &= 0x1F;
            if(this->rockmanMapper) {
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
void Cartridge::m2w(u16 addr, u8 val) {
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
void Cartridge::m3w(u16 addr, u8 val) {
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

                if(this->gbClock.days > 0x1FF) {
                    this->mbc3Ctrl |= 0x80;
                    this->gbClock.days &= 0x1FF;
                }

                this->mbc3Ctrl = (u8) ((this->mbc3Ctrl & ~1) | ((this->gbClock.days >> 8) & 1));
            }

            break;
        case 0xA: /* A000 - BFFF */
        case 0xB:
            if(!this->sramEnabled) {
                break;
            }

            switch(this->sramBank) { // Check for RTC register
                case 0x8:
                    if(this->gbClock.seconds != val) {
                        this->gbClock.seconds = val;
                    }

                    return;
                case 0x9:
                    if(this->gbClock.minutes != val) {
                        this->gbClock.minutes = val;
                    }

                    return;
                case 0xA:
                    if(this->gbClock.hours != val) {
                        this->gbClock.hours = val;
                    }

                    return;
                case 0xB:
                    if((this->gbClock.days & 0xff) != val) {
                        this->gbClock.days &= 0x100;
                        this->gbClock.days |= val;
                    }

                    return;
                case 0xC:
                    if(this->mbc3Ctrl != val) {
                        this->gbClock.days &= 0xFF;
                        this->gbClock.days |= (val & 1) << 8;
                        this->mbc3Ctrl = val;
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
void Cartridge::m5w(u16 addr, u8 val) {
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
            if(this->rumble) {
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
void Cartridge::m6w(u16 addr, u8 val) {
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
void Cartridge::m7w(u16 addr, u8 val) {
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
void Cartridge::mmm01w(u16 addr, u8 val) {
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
                this->mmm01RomBaseBank = (u8) (((val & 0x3F) % this->getRomBanks()) + 2);
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
void Cartridge::h1w(u16 addr, u8 val) {
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
void Cartridge::h3w(u16 addr, u8 val) {
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
                                case 8: /* Minutes */
                                    this->huc3Value = (u8) ((this->gbClock.minutes >> this->huc3Shift) & 0xF);
                                    break;
                                case 12:
                                case 16:
                                case 20: /* Days */
                                    this->huc3Value = (u8) ((this->gbClock.days >> (this->huc3Shift - 12)) & 0xF);
                                    break;
                                case 24: /* Year */
                                    this->huc3Value = (u8) (this->gbClock.years & 0xF);
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
void Cartridge::camw(u16 addr, u8 val) {
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
void Cartridge::t5w(u16 addr, u8 val) {
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
                        u8 data = (u8) ((this->tama5Commands[0x04] & 0xF) | ((this->tama5Commands[0x05] & 0xF) << 4));
                        if(this->tama5RamByteSelect == 0x8) {
                            switch (data & 0xF) {
                                case 0x7:
                                    this->gbClock.days = (this->gbClock.days / 10) * 10 + (data >> 4);
                                    break;
                                case 0x8:
                                    this->gbClock.days = (this->gbClock.days % 10) + (data >> 4) * 10;
                                    break;
                                case 0x9:
                                    this->gbClock.months = (this->gbClock.months / 10) * 10 + (data >> 4);
                                    break;
                                case 0xa:
                                    this->gbClock.months = (this->gbClock.months % 10) + (data >> 4) * 10;
                                    break;
                                case 0xb:
                                    this->gbClock.years = (this->gbClock.years % 1000) + (data >> 4) * 1000;
                                    break;
                                case 0xc:
                                    this->gbClock.years = (this->gbClock.years % 100) + (this->gbClock.years / 1000) * 1000 + (data >> 4) * 100;
                                    break;
                                default:
                                    break;
                            }
                        } else if(this->tama5RamByteSelect == 0x18) {
                            this->latchClock();

                            u32 seconds = (this->gbClock.seconds / 10) * 16 + this->gbClock.seconds % 10;
                            u32 secondsL = (this->gbClock.seconds % 10);
                            u32 secondsH = (this->gbClock.seconds / 10);
                            u32 minutes = (this->gbClock.minutes / 10) * 16 + this->gbClock.minutes % 10;
                            u32 hours = (this->gbClock.hours / 10) * 16 + this->gbClock.hours % 10;
                            u32 daysL = this->gbClock.days % 10;
                            u32 daysH = this->gbClock.days / 10;
                            u32 monthsL = this->gbClock.months % 10;
                            u32 monthsH = this->gbClock.months / 10;
                            u32 years3 = (this->gbClock.years / 100) % 10;
                            u32 years4 = (this->gbClock.years / 1000);

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
                                this->gbClock.years = ((this->gbClock.years >> 2) << 2) + (data & 3);
                            }
                        } else if(this->tama5RamByteSelect == 0x44) {
                            this->gbClock.minutes = (u32) ((data >> 4) * 10 + (data & 0xF));
                        } else if(this->tama5RamByteSelect == 0x54) {
                            this->gbClock.hours = (u32) ((data >> 4) * 10 + (data & 0xF));
                        } else {
                            this->tama5RAM[this->tama5RamByteSelect] = data;
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

void Cartridge::camu() {
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

void Cartridge::camTakePicture() {
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
#define OVERFLOW_VAL(x, val, y) ({ \
    while ((x) >= (val)) {         \
        (x) -= (val);              \
        (y)++;                     \
    }                              \
})

static u32 daysInMonth[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static u32 daysInLeapMonth[12] = {
        31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

void Cartridge::latchClock() {
    time_t now;
    time(&now);

    time_t difference = (time_t) (now - this->gbClock.last);
    struct tm* lt = gmtime((const time_t*) &difference);

    this->gbClock.seconds += lt->tm_sec;
    OVERFLOW_VAL(this->gbClock.seconds, 60, this->gbClock.minutes);
    this->gbClock.minutes += lt->tm_min;
    OVERFLOW_VAL(this->gbClock.minutes, 60, this->gbClock.hours);
    this->gbClock.hours += lt->tm_hour;
    OVERFLOW_VAL(this->gbClock.hours, 24, this->gbClock.days);
    this->gbClock.days += lt->tm_mday - 1;
    OVERFLOW_VAL(this->gbClock.days, (this->gbClock.years & 3) == 0 ? daysInLeapMonth[this->gbClock.months] : daysInMonth[this->gbClock.months], this->gbClock.months);
    this->gbClock.months += lt->tm_mon;
    OVERFLOW_VAL(this->gbClock.months, 12, this->gbClock.years);
    this->gbClock.years += lt->tm_year - 70;

    this->gbClock.last = (u64) now;
}
