#include <sys/stat.h>
#include <string.h>

#include <string>
#include <algorithm>

#include "platform/system.h"
#include "apu.h"
#include "cpu.h"
#include "gameboy.h"
#include "mbc.h"
#include "mmu.h"
#include "romfile.h"

RomFile::RomFile(Gameboy* gb, const std::string path) {
    this->file = fopen(path.c_str(), "rb");
    if(this->file == NULL) {
        this->loaded = false;
        return;
    }

    this->gameboy = gb;

    this->fileName = path;
    std::string::size_type dot = this->fileName.find_last_of('.');
    if(dot != std::string::npos) {
        this->fileName = this->fileName.substr(0, dot);
    }

    struct stat st;
    fstat(fileno(this->file), &st);
    u32 size = (u32) st.st_size;

    if(size >= 0x70) {
        char gbsMagic[4] = {0};
        fread(gbsMagic, 1, sizeof(gbsMagic), this->file);
        fseek(this->file, 0, SEEK_SET);

        if(gbsMagic[0] == 'G' && gbsMagic[1] == 'B' && gbsMagic[2] == 'S' && gbsMagic[3] == 1) {
            u8* gbsHeader = new u8[0x70]();
            fread(gbsHeader, 1, 0x70, this->file);

            this->gbsInfo = new GBS(this, gbsHeader);
            size -= 0x70;

            delete gbsHeader;
        }
    }

    // Round number of banks to next power of two.
    this->totalRomBanks = (int) ((size + 0x3FFF) / 0x4000);
    this->totalRomBanks--;
    this->totalRomBanks |= this->totalRomBanks >> 1;
    this->totalRomBanks |= this->totalRomBanks >> 2;
    this->totalRomBanks |= this->totalRomBanks >> 4;
    this->totalRomBanks |= this->totalRomBanks >> 8;
    this->totalRomBanks |= this->totalRomBanks >> 16;
    this->totalRomBanks++;

    this->banks = new u8*[this->totalRomBanks]();

    // Most MMM01 dumps have the initial banks at the end of the ROM rather than the beginning, so check if this is the case and compensate.
    if(this->gbsInfo == NULL && this->totalRomBanks > 2) {
        // Check for the logo.
        static const u8 logo[] = {
                0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
                0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
                0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC ,0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
        };

        u8 romLogo[sizeof(logo)];
        fseek(this->file, -0x8000 + 0x0104, SEEK_END);
        fread(&romLogo, 1, sizeof(romLogo), this->file);

        if(memcmp(logo, romLogo, sizeof(logo)) == 0) {
            // Check for MMM01.
            u8 mbcType;
            fseek(this->file, -0x8000 + 0x0147, SEEK_END);
            fread(&mbcType, 1, sizeof(mbcType), this->file);

            if(mbcType >= 0x0B && mbcType <= 0x0D) {
                this->firstBanksAtEnd = true;
            }
        }
    }

    u8* bank0 = this->getRomBank(0);
    if(bank0 == NULL) {
        this->loaded = false;
        return;
    }

    this->romTitle = std::string(reinterpret_cast<char*>(&bank0[0x0134]), bank0[0x0143] == 0x80 || bank0[0x0143] == 0xC0 ? 15 : 16);
    this->romTitle.erase(std::find_if(this->romTitle.rbegin(), this->romTitle.rend(), [](int c) { return c != 0; }).base(), this->romTitle.end());

    this->cgbSupported = bank0[0x0143] == 0x80 || bank0[0x0143] == 0xC0;
    this->cgbRequired = bank0[0x0143] == 0xC0;
    this->sgb = bank0[0x146] == 0x03 && bank0[0x014B] == 0x33;

    this->rawMBC = this->gbsInfo == NULL ? bank0[0x0147] : (u8) 0x19;
    switch(this->rawMBC) {
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
            systemPrintDebug("Unsupported mapper value %02x\n", bank0[0x0147]);
            this->mbcType = MBC5;
            break;
    }

    this->rawRomSize = bank0[0x0148];
    this->rawRamSize = this->mbcType != MBC2 && this->mbcType != MBC7 && this->mbcType != TAMA5 && this->gbsInfo == NULL ? bank0[0x0149] : (u8) 1;
    switch(this->rawRamSize) {
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
            systemPrintDebug("Invalid RAM bank number: %x\nDefaulting to 4 banks.\n", this->rawRamSize);
            this->totalRamBanks = 4;
            break;
    }
}

RomFile::~RomFile() {
    if(this->banks != NULL) {
        for(int i = 0; i < this->totalRomBanks; i++) {
            if(this->banks[i] != NULL) {
                delete this->banks[i];
                this->banks[i] = NULL;
            }
        }

        delete this->banks;
        this->banks = NULL;
    }

    if(this->gbsInfo != NULL) {
        delete this->gbsInfo;
        this->gbsInfo = NULL;
    }

    if(this->file != NULL) {
        fclose(this->file);
        this->file = NULL;
    }
}

u8* RomFile::getRomBank(int bank) {
    if(bank < 0 || bank >= this->totalRomBanks) {
        return NULL;
    }

    if(this->banks[bank] == NULL) {
        this->banks[bank] = new u8[0x4000]();

        u32 baseAddress = 0;
        if(this->gbsInfo != NULL) {
            if(bank == 0) {
                fseek(this->file, 0x70, SEEK_SET);
                baseAddress = this->gbsInfo->getLoadAddress();
            } else {
                fseek(this->file, 0x70 + (0x4000 - this->gbsInfo->getLoadAddress()) + ((bank - 1) * 0x4000), SEEK_SET);
            }
        } else if(this->firstBanksAtEnd) {
            if(bank < 2) {
                fseek(this->file, -((2 - bank) * 0x4000), SEEK_END);
            } else {
                fseek(this->file, (bank - 2) * 0x4000, SEEK_SET);
            }
        } else {
            fseek(this->file, bank * 0x4000, SEEK_SET);
        }

        fread(this->banks[bank] + baseAddress, 1, (size_t) (0x4000 - baseAddress), this->file);
    }

    return this->banks[bank];
}

GBS::GBS(RomFile* romFile, u8* header) {
    this->rom = romFile;

    this->songCount = header[0x04];
    this->firstSong = header[0x05] - (u8) 1;
    this->loadAddress = header[0x06] | (header[0x07] << 8);
    this->initAddress = header[0x08] | (header[0x09] << 8);
    this->playAddress = header[0x0A] | (header[0x0B] << 8);
    this->stackPointer = header[0x0C] | (header[0x0D] << 8);
    this->timerModulo = header[0x0E];
    this->timerControl = header[0x0F];

    this->title = std::string(reinterpret_cast<char*>(&header[0x10]), 0x20);
    this->title.erase(std::find_if(this->title.rbegin(), this->title.rend(), [](int c) { return c != 0; }).base(), this->title.end());

    this->author = std::string(reinterpret_cast<char*>(&header[0x30]), 0x20);
    this->author.erase(std::find_if(this->author.rbegin(), this->author.rend(), [](int c) { return c != 0; }).base(), this->author.end());

    this->copyright = std::string(reinterpret_cast<char*>(&header[0x50]), 0x20);
    this->copyright.erase(std::find_if(this->copyright.rbegin(), this->copyright.rend(), [](int c) { return c != 0; }).base(), this->copyright.end());
}

void GBS::playSong(Gameboy* gameboy, int song) {
    gameboy->reset();

    u8* bank0 = this->rom->getRomBank(0);
    bool vblank = this->timerModulo == 0 && (this->timerControl & 0x7F) == 0;

    // RST vectors.
    for(u16 vector = 0; vector < 8; vector++) {
        u16 base = (u16) (vector * 8);
        u16 dest = (u16) (this->loadAddress + vector * 8);

        // JP dest;
        bank0[base + 0] = 0xC3;
        bank0[base + 1] = (u8) (dest & 0xFF);
        bank0[base + 2] = (u8) (dest >> 8);
    }

    // Interrupt handlers.
    for(u8 handler = 0; handler < 5; handler++) {
        u16 base = (u16) (0x40 + handler * 8);

        if((vblank && handler == 0) || (!vblank && handler == 2)) {
            // CALL playAddress; RETI;
            bank0[base + 0] = 0xCD;
            bank0[base + 1] = (u8) (this->playAddress & 0xFF);
            bank0[base + 2] = (u8) (this->playAddress >> 8);
            bank0[base + 3] = 0xD9;
        } else {
            // RETI;
            bank0[base] = 0xD9;
        }
    }

    // LD SP, stackPointer; LD A, song; CALL initAddress; EI; HALT; JR -3;
    bank0[0x100] = 0x31;
    bank0[0x101] = (u8) (this->stackPointer & 0xFF);
    bank0[0x102] = (u8) (this->stackPointer >> 8);
    bank0[0x103] = 0x3E;
    bank0[0x104] = (u8) song;
    bank0[0x105] = 0xCD;
    bank0[0x106] = (u8) (this->initAddress & 0xFF);
    bank0[0x107] = (u8) (this->initAddress >> 8);
    bank0[0x108] = 0xFB;
    bank0[0x109] = 0x76;
    bank0[0x10A] = 0x18;
    bank0[0x10B] = (u8) -3;

    if(this->timerControl & 0x80) {
        gameboy->cpu->setDoubleSpeed(true);
    }

    gameboy->mmu->write(0xFF05, 0x00);
    gameboy->mmu->write(0xFF06, this->timerModulo);
    gameboy->mmu->write(0xFF07, (u8) (this->timerControl & 0x7F));
    gameboy->mmu->write(0xFFFF, (u8) (vblank ? INT_VBLANK : INT_TIMER));
}

void GBS::stopSong(Gameboy* gameboy) {
    gameboy->reset();

    u8* bank0 = this->rom->getRomBank(0);

    // JR -2;
    bank0[0x100] = 0x18;
    bank0[0x101] = (u8) -2;
}