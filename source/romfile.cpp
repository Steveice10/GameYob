#include <sys/stat.h>
#include <string.h>

#include <string>
#include <algorithm>

#include "platform/system.h"
#include "gameboy.h"
#include "mmu.h"
#include "romfile.h"

RomFile::RomFile(u8* rom, u32 size) {
    // Round number of banks to next power of two.
    this->totalRomBanks = (int) ((size + 0x3FFF) / 0x4000);
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

    this->rom = new u8[this->totalRomBanks * 0x4000];

    // Most MMM01 dumps have the initial banks at the end of the ROM rather than the beginning, so check if this is the case and compensate.
    bool initialized = false;
    if(size > 0x8000) {
        // Check for the logo.
        static const u8 logo[] = {
                0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
                0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
                0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC ,0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
        };

        u8* banks = &rom[size - 0x8000];

        if(memcmp(logo, &banks[0x0104], sizeof(logo)) == 0) {
            // Check for MMM01.
            u8 mbcType = banks[0x0147];

            if(mbcType >= 0x0B && mbcType <= 0x0D) {
                memcpy(this->rom, banks, 0x8000);
                memcpy(&this->rom[0x8000], rom, size - 0x8000);

                initialized = true;
            }
        }
    }

    if(!initialized) {
        memcpy(this->rom, rom, size);
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
}

RomFile::~RomFile() {
    if(this->rom != NULL) {
        delete this->rom;
        this->rom = NULL;
    }
}