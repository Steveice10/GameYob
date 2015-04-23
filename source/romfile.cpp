#include <sys/stat.h>
#include <math.h>
#include <string.h>
#include <string>
#include <algorithm>

#include "ui/menu.h"
#include "gameboy.h"
#include "romfile.h"

RomFile::RomFile(Gameboy* gb, const std::string path) {
    this->file = fopen(path.c_str(), "r");
    if(!this->file) {
        this->loaded = false;
        return;
    }

    this->gameboy = gb;
    this->gameboy->getGBSPlayer()->gbsMode = (strcasecmp(strrchr(path.c_str(), '.'), ".gbs") == 0);

    this->fileName = path;
    std::string::size_type dot = this->fileName.find_last_of('.');
    if(dot != std::string::npos) {
        this->fileName = this->fileName.substr(0, dot);
    }

    struct stat st;
    fstat(fileno(this->file), &st);

    int sizeMod = 0;
    if(gameboy->getGBSPlayer()->gbsMode) {
        fread(gameboy->getGBSPlayer()->gbsHeader, 1, 0x70, this->file);
        gameboy->getGBSPlayer()->gbsReadHeader();

        sizeMod = -0x70;
    }

    this->totalRomBanks = (int) pow(2, ceil(log(((u32) st.st_size + 0x3FFF + sizeMod) / 0x4000) / log(2)));
    this->banks = new u8*[this->totalRomBanks]();

    // Most MMM01 dumps have the initial banks at the end of the ROM rather than the beginning, so check if this is the case and compensate.
    if(this->totalRomBanks > 2) {
        u8 cgbFlag;
        fseek(this->file, -0x8000 + 0x0143, SEEK_END);
        fread(&cgbFlag, 1, sizeof(cgbFlag), this->file);

        char buffer[cgbFlag == 0x80 || cgbFlag == 0xC0 ? 15 : 16] = {'\0'};
        fseek(this->file, -0x8000 + 0x0134, SEEK_END);
        fread(&buffer, 1, sizeof(buffer), this->file);

        if(strcmp(buffer, "VARIETY PACK") == 0 || strcmp(buffer, "MOMOCOL2") == 0 || strcmp(buffer, "BUBBLEBOBBLE SET") == 0 || strcmp(buffer, "TETRIS SET") == 0 || strcmp(buffer, "GANBARUGA SET") == 0) {
            this->firstBanksAtEnd = true;
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
    this->sgb = bank0[0x014b] == 0x33 && bank0[0x146] == 0x03;

    this->rawMBC = !this->gameboy->getGBSPlayer()->gbsMode ? bank0[0x0147] : (u8) 0x19;
    switch(this->rawMBC) {
        case 0x00:
        case 0x08:
        case 0x09:
            this->mbc = MBC0;
            break;
        case 0x01:
        case 0x02:
        case 0x03:
            this->mbc = MBC1;
            break;
        case 0x05:
        case 0x06:
            this->mbc = MBC2;
            break;
        case 0x0B:
        case 0x0C:
        case 0x0D:
            this->mbc = MMM01;
            break;
        case 0x0F:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            this->mbc = MBC3;
            break;
        case 0x19:
        case 0x1A:
        case 0x1B:
            this->mbc = MBC5;
            break;
        case 0x1C:
        case 0x1D:
        case 0x1E:
            this->mbc = MBC5;
            this->rumble = true;
            break;
        case 0x22:
            this->mbc = MBC7;
            break;
        case 0xEA: // Hack for SONIC5
            this->mbc = MBC1;
            break;
        case 0xFE:
            this->mbc = HUC3;
            break;
        case 0xFF:
            this->mbc = HUC1;
            break;
        default:
            if(showConsoleDebug()) {
                printf("Unsupported mapper value %02x\n", bank0[0x0147]);
            }

            this->mbc = MBC5;
            break;
    }

    this->rawRomSize = bank0[0x0148];
    this->rawRamSize = this->mbc != MBC2 && this->mbc != MBC7 && !this->gameboy->getGBSPlayer()->gbsMode ? bank0[0x0149] : (u8) 1;
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
            if(showConsoleDebug()) {
                printf("Invalid RAM bank number: %x\nDefaulting to 4 banks.\n", this->rawRamSize);
            }

            this->totalRamBanks = 4;
            break;
    }
}

RomFile::~RomFile() {
    for(int i = 0; i < this->totalRomBanks; i++) {
        if(this->banks[i] != NULL) {
            delete this->banks[i];
            this->banks[i] = NULL;
        }
    }

    if(this->banks != NULL) {
        delete this->banks;
        this->banks = NULL;
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

        if(bank == 0 && this->gameboy->getGBSPlayer()->gbsMode) {
            fseek(this->file, 0x70, SEEK_SET);
            fread(this->banks[bank] + this->gameboy->getGBSPlayer()->gbsLoadAddress, 1, (size_t) (0x4000 - this->gameboy->getGBSPlayer()->gbsLoadAddress), this->file);
        } else {
            if(this->firstBanksAtEnd) {
                if(bank < 2) {
                    fseek(this->file, -((2 - bank) * 0x4000), SEEK_END);
                } else {
                    fseek(this->file, (bank - 2) * 0x4000, SEEK_SET);
                }
            } else {
                fseek(this->file, bank * 0x4000, SEEK_SET);
            }

            fread(this->banks[bank], 1, 0x4000, this->file);
        }

        this->gameboy->getCheatEngine()->applyGGCheatsToBank(bank);
    }

    return this->banks[bank];
}

void RomFile::printInfo() {
    static const char* mbcNames[] = {"ROM", "MBC1", "MBC2", "MBC3", "MBC5", "MBC7", "MMM01", "HUC1", "HUC3"};

    iprintf("\x1b[2J");
    printf("Cartridge type: %.2x (%s)\n", this->rawMBC, mbcNames[this->mbc]);
    printf("ROM Title: \"%s\"\n", this->romTitle.c_str());
    printf("ROM Size: %.2x (%d banks)\n", this->rawRomSize, this->totalRomBanks);
    printf("RAM Size: %.2x (%d banks)\n", this->rawRamSize, this->totalRamBanks);
    printf("CGB: Supported: %d, Required: %d\n", this->cgbSupported, this->cgbRequired);
}
