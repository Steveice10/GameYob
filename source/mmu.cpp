#include <stdio.h>
#include <string.h>

#include "gb_apu/Gb_Apu.h"

#include "platform/system.h"
#include "gameboy.h"
#include "ppu.h"
#include "romfile.h"

#define refreshVramBank() { \
    memory[0x8] = vram[vramBank]; \
    memory[0x9] = vram[vramBank]+0x1000; }
#define refreshWramBank() { \
    memory[0xd] = wram[wramBank]; }

typedef void (Gameboy::*mbcWrite)(u16, u8);
typedef u8   (Gameboy::*mbcRead )(u16);

const mbcRead mbcReads[] = {
        NULL,
        NULL,
        NULL,
        &Gameboy::m3r,
        NULL,
        &Gameboy::m7r,
        NULL,
        NULL,
        &Gameboy::h3r,
        &Gameboy::camr,
        NULL
};

const mbcWrite mbcWrites[] = {
        &Gameboy::m0w,
        &Gameboy::m1w,
        &Gameboy::m2w,
        &Gameboy::m3w,
        &Gameboy::m5w,
        &Gameboy::m7w,
        &Gameboy::mmm01w,
        &Gameboy::h1w,
        &Gameboy::h3w,
        &Gameboy::camw,
        &Gameboy::t5w
};

void Gameboy::refreshRomBank0(int bank) {
    if(bank < romFile->getRomBanks()) {
        romBank0Num = bank;
        u8* romBank = romFile->getRomBank(romBank0Num);
        memory[0x0] = romBank;
        memory[0x1] = romBank + 0x1000;
        memory[0x2] = romBank + 0x2000;
        memory[0x3] = romBank + 0x3000;
    } else {
        systemPrintDebug("Tried to access ROM bank %x\n", bank);
    }
}

void Gameboy::refreshRomBank1(int bank) {
    if(bank < romFile->getRomBanks()) {
        romBank1Num = bank;
        u8* romBank = romFile->getRomBank(romBank1Num);
        memory[0x4] = romBank;
        memory[0x5] = romBank + 0x1000;
        memory[0x6] = romBank + 0x2000;
        memory[0x7] = romBank + 0x3000;
    } else {
        systemPrintDebug("Tried to access ROM bank %x\n", bank);
    }
}

void Gameboy::refreshRamBank(int bank) {
    if(bank < romFile->getRamBanks()) {
        ramBankNum = bank;
        u8* ramBank = externRam + ramBankNum * 0x2000;
        memory[0xa] = ramBank;
        memory[0xb] = ramBank + 0x1000;
    } else {
        systemPrintDebug("Tried to access RAM bank %x\n", bank);
    }
}

void Gameboy::writeSram(u16 addr, u8 val) {
    if(externRam != NULL) {
        int pos = addr + ramBankNum * 0x2000;
        if(externRam[pos] != val) {
            externRam[pos] = val;
            if(autosaveEnabled) {
                saveModified = true;
                dirtySectors[pos / 512] = true;
            }
        }
    } else {
        systemPrintDebug("Tried to access RAM when none exists.\n");
    }
}

void Gameboy::initMMU() {
    if(romFile == NULL) {
        return;
    }

    wramBank = 1;
    vramBank = 0;
    romBank0Num = 0;
    romBank1Num = 1;
    ramBankNum = 0;

    memoryModel = 0;
    dmaSource = 0;
    dmaDest = 0;
    dmaLength = 0;
    dmaMode = 0;

    ramEnabled = false;

    HuC3Value = 0;
    HuC3Shift = 0;

    /* Rockman8 by Yang Yang uses a silghtly different MBC1 variant */
    rockmanMapper = romFile->getRomTitle().compare("ROCKMAN 99") == 0;

    readFunc = mbcReads[romFile->getMBC()];
    writeFunc = mbcWrites[romFile->getMBC()];

    mapMemory();
    for(int i = 0; i < 8; i++) {
        memset(wram[i], 0, 0x1000);
    }

    memset(highram, 0, 0x1000); // Initializes sprites and IO registers

    writeIO(0x02, 0x00);
    writeIO(0x05, 0x00);
    writeIO(0x06, 0x00);
    writeIO(0x07, 0x00);
    writeIO(0x40, 0x91);
    writeIO(0x42, 0x00);
    writeIO(0x43, 0x00);
    writeIO(0x45, 0x00);
    writeIO(0x47, 0xfc);
    writeIO(0x48, 0xff);
    writeIO(0x49, 0xff);
    writeIO(0x4a, 0x00);
    writeIO(0x4b, 0x00);
    writeIO(0xff, 0x00);

    ioRam[0x55] = 0xff;

    memset(dirtySectors, 0, sizeof(dirtySectors));

    mbc7WriteEnable = false;
    mbc7Idle = false;
    mbc7Cs = 0;
    mbc7Sk = 0;
    mbc7OpCode = 0;
    mbc7Addr = 0;
    mbc7Count = 0;
    mbc7State = 0;
    mbc7Buffer = 0;
    mbc7RA = 0; // Ram Access register 0xa080

    mmm01BankSelected = false;
    mmm01RomBaseBank = 0;

    cameraIO = false;

    tama5CommandNumber = 0;
    tama5RamByteSelect = 0;
    memset(tama5Commands, 0, sizeof(tama5Commands));
    memset(tama5RAM, 0, sizeof(tama5RAM));
}

void Gameboy::mapMemory() {
    refreshRomBank0(romBank0Num);
    if(biosOn) {
        u8* bios = gbcBios;
        if(biosMode == 1) {
            bios = resultantGBMode != 1 && gbBiosLoaded ? (u8*) gbBios : (u8*) gbcBios;
        } else if(biosMode == 2) {
            bios = gbBios;
        } else if(biosMode == 3) {
            bios = gbcBios;
        }

        memory[0x0] = bios;

        // Little hack to preserve "quickRead" from gbcpu.cpp.
        memcpy(bios + 0x100, romFile->getRomBank(0) + 0x100, 0x50);
    }

    refreshRomBank1(romBank1Num);
    if(romFile->getRamBanks() > 0) {
        refreshRamBank(ramBankNum);
    }

    refreshVramBank();
    memory[0xc] = wram[0];
    refreshWramBank();
    memory[0xe] = wram[0];
    memory[0xf] = highram;

    dmaSource = (ioRam[0x51] << 8) | (ioRam[0x52]);
    dmaSource &= 0xFFF0;
    dmaDest = (ioRam[0x53] << 8) | (ioRam[0x54]);
    dmaDest &= 0x1FF0;
}

u8 Gameboy::readIO(u8 ioReg) {
    switch(ioReg) {
        case 0x00:
            return sgbReadP1();
        // APU registers.
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x1F:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2E:
        case 0x2F:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            return (u8) apu->read_register(soundCycles, 0xFF00 + ioReg);
        case 0x56:
            return ioRam[ioReg] | (u8) ((ioRam[ioReg] & 0xC0) && systemGetIRState() ? 0 : (1 << 1));
        case 0x70: // wram register
            return ioRam[ioReg] | 0xf8;
        default:
            return ioRam[ioReg];
    }
}

u8 Gameboy::readMemoryOther(u16 addr) {
    int area = addr >> 12;

    if(area == 0xf) {
        if(addr >= 0xff00) {
            return readIO(addr & 0xff);
        } else if(addr < 0xfe00) { // Check for echo area
            addr -= 0x2000;
        }
    } else if(area == 0xa || area == 0xb) { /* Check if in range a000-bfff */
        /* Check if there's an handler for this mbc */
        if(readFunc != NULL) {
            return (*this.*readFunc)(addr);
        } else if(romFile->getRamBanks() == 0) {
            return 0xff;
        }
    }

    return memory[area][addr & 0xfff];
}

void Gameboy::writeMemoryOther(u16 addr, u8 val) {
    int area = addr >> 12;
    switch(area) {
        case 0x8:
        case 0x9:
            ppu->writeVram(addr & 0x1fff, val);
            vram[vramBank][addr & 0x1fff] = val;
            return;
        case 0xE: // Echo area
            wram[0][addr & 0xFFF] = val;
            return;
        case 0xF:
            if(addr >= 0xFF00) {
                writeIO(addr & 0xFF, val);
            } else if(addr >= 0xFE00) {
                ppu->writeHram(addr & 0x1ff, val);
                hram[addr & 0x1ff] = val;
            } else {// Echo area
                wram[wramBank][addr & 0xFFF] = val;
            }

            return;
    }

    if(writeFunc != NULL) {
        (*this.*writeFunc)(addr, val);
    }
}

void Gameboy::writeIO(u8 ioReg, u8 val) {
    switch(ioReg) {
        case 0x00:
            if(sgbMode) {
                sgbHandleP1(val);
            } else {
                ioRam[0x00] = val;
            }

            return;
        case 0x01:
            ioRam[ioReg] = val;
            return;
        case 0x02: {
            ioRam[ioReg] = val;
            if(val & 0x80 && val & 0x01) {
                if(serialCounter == 0) {
                    serialCounter = clockSpeed / 1024;
                    if(cyclesToExecute > serialCounter) {
                        cyclesToExecute = serialCounter;
                    }
                }
            } else {
                serialCounter = 0;
            }

            return;
        }
        case 0x04:
            ioRam[ioReg] = 0;
            return;
        case 0x05:
            ioRam[ioReg] = val;
            return;
        case 0x06:
            ioRam[ioReg] = val;
            return;
        case 0x07:
            timerPeriod = periods[val & 0x3];
            ioRam[ioReg] = val;
            return;
        // APU registers.
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x1F:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2E:
        case 0x2F:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            apu->write_register(soundCycles, 0xFF00 + ioReg, val);
            ioRam[ioReg] = val;
            return;
        case 0x42:
        case 0x43:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
            ppu->handleVideoRegister(ioReg, val);
            ioRam[ioReg] = val;
            return;
        case 0x56:
            systemSetIRState((val & (1 << 0)) == 1);
            ioRam[ioReg] = (u8) (val & ~(1 << 1));
            return;
        case 0x69: // CGB BG Palette
            ppu->handleVideoRegister(ioReg, val);
            bgPaletteData[ioRam[0x68] & 0x3F] = val;
            if(ioRam[0x68] & 0x80) {
                ioRam[0x68] = 0x80 | (ioRam[0x68] + 1);
            }

            ioRam[0x69] = bgPaletteData[ioRam[0x68] & 0x3F];
            return;
        case 0x6B: // CGB Sprite palette
            ppu->handleVideoRegister(ioReg, val);
            sprPaletteData[ioRam[0x6A] & 0x3F] = val;
            if(ioRam[0x6A] & 0x80) {
                ioRam[0x6A] = 0x80 | (ioRam[0x6A] + 1);
            }

            ioRam[0x6B] = sprPaletteData[ioRam[0x6A] & 0x3F];
            return;
        case 0x46: { // Sprite DMA
            ppu->handleVideoRegister(ioReg, val);
            ioRam[ioReg] = val;

            int src = val << 8;
            if((src >> 12) == ((src + 0x9F) >> 12)) {
                memcpy(hram, &memory[src >> 12][src & 0xFFF], 0xA0);
            } else {
                int part1src = src;
                int part1size = 0x1000 - (src & 0xFFF);
                int part2src = src + part1size;
                int part2size = 0xA0 - part1size;

                memcpy(hram, &memory[part1src >> 12][part1src & 0xFFF], (size_t) part1size);
                memcpy(&hram[part1size], &memory[part2src >> 12][part2src & 0xFFF], (size_t) part2size);
            }

            return;
        }
        case 0x40: // LCDC
            ppu->handleVideoRegister(ioReg, val);
            ioRam[ioReg] = val;
            if(!(val & 0x80)) {
                ioRam[0x44] = 0;
                ioRam[0x41] &= ~3; // Set video mode 0
            }

            return;
        case 0x41:
            ioRam[ioReg] &= 0x7;
            ioRam[ioReg] |= val & 0xF8;
            return;
        case 0x44:
            //ioRam[0x44] = 0;
            systemPrintDebug("LY Write %d.\n", val);
            return;
        case 0x45:
            ioRam[ioReg] = val;
            checkLYC();
            return;
        case 0x68:
            ioRam[ioReg] = val;
            ioRam[0x69] = bgPaletteData[val & 0x3F];
            return;
        case 0x6A:
            ioRam[ioReg] = val;
            ioRam[0x6B] = sprPaletteData[val & 0x3F];
            return;
        case 0x4D:
            ioRam[ioReg] &= 0x80;
            ioRam[ioReg] |= (val & 1);
            return;
        case 0x4F: // Vram bank
            if(gbMode == CGB) {
                vramBank = val & 1;
                refreshVramBank();
            }

            ioRam[ioReg] = val & 1;
            return;
        case 0x50: // BIOS Lockdown
            biosOn = false;
            initGameboyMode();

            if(gbMode == GB) {
                // Reinitialize sound so that it'll be in GB mode.
                initSND();
            }

            refreshRomBank0(romBank0Num);
            return;
        case 0x55: // CGB DMA
            if(gbMode == CGB) {
                if(dmaLength > 0) {
                    if((val & 0x80) == 0) {
                        ioRam[ioReg] |= 0x80;
                        dmaLength = 0;
                    }

                    return;
                }

                dmaLength = ((val & 0x7F) + 1);
                dmaSource = (ioRam[0x51] << 8) | (ioRam[0x52]);
                dmaSource &= 0xFFF0;
                dmaDest = (ioRam[0x53] << 8) | (ioRam[0x54]);
                dmaDest &= 0x1FF0;
                dmaMode = val >> 7;
                ioRam[ioReg] = dmaLength - 1;
                if(dmaMode == 0) {
                    for(int i = 0; i < dmaLength; i++) {
                        ppu->writeVram16(dmaDest, dmaSource);
                        for(int j = 0; j < 16; j++) {
                            vram[vramBank][dmaDest++] = quickRead(dmaSource++);
                        }

                        dmaDest &= 0x1FF0;
                    }

                    extraCycles += dmaLength * 8 * (doubleSpeed + 1);
                    dmaLength = 0;
                    ioRam[ioReg] = 0xFF;
                    ioRam[0x51] = dmaSource >> 8;
                    ioRam[0x52] = dmaSource & 0xff;
                    ioRam[0x53] = dmaDest >> 8;
                    ioRam[0x54] = dmaDest & 0xff;
                }
            } else {
                ioRam[ioReg] = val;
            }

            return;
        case 0x70:                // WRAM bank, for CGB only
            if(gbMode == CGB) {
                wramBank = val & 0x7;
                if(wramBank == 0) {
                    wramBank = 1;
                }

                refreshWramBank();
            }

            ioRam[ioReg] = val & 0x7;
            return;
        case 0x0F: // IF
            ioRam[ioReg] = val;
            interruptTriggered = val & ioRam[0xff];
            if(interruptTriggered) {
                cyclesToExecute = -1;
            }

            break;
        case 0xFF: // IE
            ioRam[ioReg] = val;
            interruptTriggered = val & ioRam[0x0f];
            if(interruptTriggered) {
                cyclesToExecute = -1;
            }

            break;
        default:
            ioRam[ioReg] = val;
            return;
    }
}

void Gameboy::refreshP1() {
    int controller = 0;

    // Check if input register is being used for sgb packets
    if(sgbPacketBit == -1) {
        if((ioRam[0x00] & 0x30) == 0x30) {
            if(!sgbMode) {
                ioRam[0x00] |= 0x0F;
            }
        } else {
            ioRam[0x00] &= 0xF0;
            if(!(ioRam[0x00] & 0x20)) {
                ioRam[0x00] |= (controllers[controller] & 0xF);
            } else {
                ioRam[0x00] |= ((controllers[controller] & 0xF0) >> 4);
            }
        }

        ioRam[0x00] |= 0xc0;
    }
}

bool Gameboy::updateHBlankDMA() {
    if(dmaLength > 0) {
        ppu->writeVram16(dmaDest, dmaSource);
        for(int i = 0; i < 16; i++) {
            vram[vramBank][dmaDest++] = quickRead(dmaSource++);
        }

        dmaDest &= 0x1FF0;
        dmaLength--;
        ioRam[0x55] = dmaLength - 1;
        ioRam[0x51] = dmaSource >> 8;
        ioRam[0x52] = dmaSource & 0xff;
        ioRam[0x53] = dmaDest >> 8;
        ioRam[0x54] = dmaDest & 0xff;
        return true;
    } else {
        return false;
    }
}
