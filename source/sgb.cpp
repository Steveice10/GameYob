#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "ppu.h"
#include "sgb.h"

#define sgbPalettes (gameboy->ppu->getVramBank(1))
#define sgbAttrFiles (gameboy->ppu->getVramBank(1)+0x1000)

SGB::SGB(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

void SGB::reset() {
    this->p1 = 0;

    this->packetLength = 0;
    this->packetsTransferred = 0;
    this->packetBit = -1;
    this->command = 0;

    this->numControllers = 1;
    this->selectedController = 0;
    this->buttonsChecked = 0;

    this->mask = 0;

    memset(this->controllers, 0, sizeof(this->controllers));
    memset(this->map, 0, sizeof(this->map));
    memset(this->packet, 0, sizeof(this->packet));
    memset(&this->cmdData, 0, sizeof(this->cmdData));
}

void SGB::loadState(FILE* file, int version) {
    fread(&this->p1, 1, sizeof(this->p1), file);

    fread(&this->packetLength, 1, sizeof(this->packetLength), file);
    fread(&this->packetsTransferred, 1, sizeof(this->packetsTransferred), file);
    fread(&this->packetBit, 1, sizeof(this->packetBit), file);
    fread(&this->command, 1, sizeof(this->command), file);

    fread(&this->numControllers, 1, sizeof(this->numControllers), file);
    fread(&this->selectedController, 1, sizeof(this->selectedController), file);
    fread(&this->buttonsChecked, 1, sizeof(this->buttonsChecked), file);

    fread(&this->mask, 1, sizeof(this->mask), file);

    fread(&this->controllers, 1, sizeof(this->controllers), file);
    fread(this->map, 1, sizeof(this->map), file);
    fread(this->packet, 1, sizeof(this->packet), file);
    fread(&this->cmdData, 1, sizeof(this->cmdData), file);
}

void SGB::saveState(FILE* file) {
    fwrite(&this->p1, 1, sizeof(this->p1), file);

    fwrite(&this->packetLength, 1, sizeof(this->packetLength), file);
    fwrite(&this->packetsTransferred, 1, sizeof(this->packetsTransferred), file);
    fwrite(&this->packetBit, 1, sizeof(this->packetBit), file);
    fwrite(&this->command, 1, sizeof(this->command), file);

    fwrite(&this->numControllers, 1, sizeof(this->numControllers), file);
    fwrite(&this->selectedController, 1, sizeof(this->selectedController), file);
    fwrite(&this->buttonsChecked, 1, sizeof(this->buttonsChecked), file);

    fwrite(&this->mask, 1, sizeof(this->mask), file);

    fwrite(&this->controllers, 1, sizeof(this->controllers), file);
    fwrite(this->map, 1, sizeof(this->map), file);
    fwrite(this->packet, 1, sizeof(this->packet), file);
    fwrite(&this->cmdData, 1, sizeof(this->cmdData), file);
}

void SGB::update() {
    if(!(this->p1 & 0x10)) {
        if((this->controllers[0] & 0xf0) != 0xf0) {
            this->gameboy->cpu->requestInterrupt(INT_JOYPAD);
        }
    } else if(!(this->p1 & 0x20)) {
        if((this->controllers[0] & 0x0f) != 0x0f) {
            this->gameboy->cpu->requestInterrupt(INT_JOYPAD);
        }
    }
}

u8 SGB::read(u16 addr) {
    switch(addr) {
        case 0xFF00: {
            u8 p1 = this->p1;

            if(this->gameboy->gbMode == MODE_SGB && (p1 & 0x30) == 0x30) {
                return (u8) (0xFF - this->selectedController);
            }

            if(!(p1 & 0x20)) {
                return (u8) (0xC0 | (p1 & 0xF0) | (this->controllers[this->selectedController] & 0xF));
            } else if(!(p1 & 0x10)) {
                return (u8) (0xC0 | (p1 & 0xF0) | ((this->controllers[this->selectedController] & 0xF0) >> 4));
            } else {
                return (u8) (p1 | 0xCF);
            }
        }
        default:
            return 0;
    }
}

void SGB::write(u16 addr, u8 val) {
    switch(addr) {
        case 0xFF00: {
            if(this->gameboy->gbMode != MODE_SGB) {
                this->p1 = val;
                return;
            }

            if((val & 0x30) == 0) {
                // Start packet transfer
                this->packetBit = 0;
                this->p1 = 0xCF;
                return;
            }

            if(this->packetBit != -1) {
                u8 oldVal = this->p1;
                this->p1 = val;

                int shift = this->packetBit % 8;
                int byte = (this->packetBit / 8) % 16;
                if(shift == 0) {
                    this->packet[byte] = 0;
                }

                int bit;
                if((oldVal & 0x30) == 0 && (val & 0x30) != 0x30) { // A bit of speculation here. Fixes Castlevania.
                    this->packetBit = -1;
                    return;
                }

                if(!(val & 0x10)) {
                    bit = 0;
                } else if(!(val & 0x20)) {
                    bit = 1;
                } else {
                    return;
                }

                this->packet[byte] |= bit << shift;
                this->packetBit++;
                if(this->packetBit == 128) {
                    if(this->packetsTransferred == 0) {
                        this->command = this->packet[0] >> 3;
                        this->packetLength = this->packet[0] & 7;
                    }

                    if(this->sgbCommands[this->command] != 0) {
                        (this->*sgbCommands[this->command])(this->packetsTransferred);
                    }

                    this->packetBit = -1;
                    this->packetsTransferred++;
                    if(this->packetsTransferred == this->packetLength) {
                        this->packetLength = 0;
                        this->packetsTransferred = 0;
                    }
                }
            } else {
                if((val & 0x30) == 0x30) {
                    if(this->buttonsChecked == 3) {
                        this->selectedController++;
                        if(this->selectedController >= this->numControllers) {
                            this->selectedController = 0;
                        }

                        this->buttonsChecked = 0;
                    }

                    this->p1 = (u8) (0xFF - this->selectedController);
                } else {
                    this->p1 = (u8) (val | 0xCF);
                    if((val & 0x30) == 0x10) {
                        this->buttonsChecked |= 1;
                    } else if((val & 0x30) == 0x20) {
                        this->buttonsChecked |= 2;
                    }
                }
            }

            break;
        }
        default:
            break;
    }
}

void SGB::loadAttrFile(int index) {
    if(index > 0x2c) {
        return;
    }

    int src = index * 90;
    int dest = 0;
    for(int i = 0; i < 20 * 18 / 4; i++) {
        this->map[dest++] = (u8) ((sgbAttrFiles[src] >> 6) & 3);
        this->map[dest++] = (u8) ((sgbAttrFiles[src] >> 4) & 3);
        this->map[dest++] = (u8) ((sgbAttrFiles[src] >> 2) & 3);
        this->map[dest++] = (u8) ((sgbAttrFiles[src] >> 0) & 3);
        src++;
    }
}

void SGB::doVramTransfer(u8* dest) {
    u8 lcdc = this->gameboy->mmu->read(0xFF40);

    int map = 0x1800 + ((lcdc >> 3) & 1) * 0x400;
    int index = 0;
    for(int y = 0; y < 18; y++) {
        for(int x = 0; x < 20; x++) {
            if(index == 0x1000) {
                return;
            }

            int tile = this->gameboy->ppu->getVramBank(0)[map + y * 32 + x];
            if(lcdc & 0x10) {
                memcpy(dest + index, this->gameboy->ppu->getVramBank(0) + tile * 16, 16);
            } else {
                memcpy(dest + index, this->gameboy->ppu->getVramBank(0) + 0x1000 + ((s8) tile) * 16, 16);
            }

            index += 16;
        }
    }
}

// Begin commands

void SGB::palXX(int block) {
    int s1, s2;
    switch(this->command) {
        case 0:
            s1 = 0;
            s2 = 1;
            break;
        case 1:
            s1 = 2;
            s2 = 3;
            break;
        case 2:
            s1 = 0;
            s2 = 3;
            break;
        case 3:
            s1 = 1;
            s2 = 2;
            break;
        default:
            return;
    }

    memcpy(this->gameboy->ppu->getBgPaletteData() + s1 * 4 + 1, this->packet + 3, 3 * sizeof(u16));
    memcpy(this->gameboy->ppu->getSprPaletteData() + s1 * 4 + 1, this->packet + 3, 3 * sizeof(u16));
    memcpy(this->gameboy->ppu->getSprPaletteData() + (s1 + 4) * 4 + 1, this->packet + 3, 3 * sizeof(u16));

    memcpy(this->gameboy->ppu->getBgPaletteData() + s2 * 4 + 1, this->packet + 9, 3 * sizeof(u16));
    memcpy(this->gameboy->ppu->getSprPaletteData() + s2 * 4 + 1, this->packet + 9, 3 * sizeof(u16));
    memcpy(this->gameboy->ppu->getSprPaletteData() + (s2 + 4) * 4 + 1, this->packet + 9, 3 * sizeof(u16));

    u16 color0 = (u16) (packet[1] | packet[2] << 8);
    for(int i = 0; i < 4; i++) {
        this->gameboy->ppu->getBgPaletteData()[i * 4] = color0;
        this->gameboy->ppu->getSprPaletteData()[i * 4] = color0;
        this->gameboy->ppu->getSprPaletteData()[(i + 4) * 4] = color0;
    }
}

void SGB::attrBlock(int block) {
    int pos;
    if(block == 0) {
        this->cmdData.attrBlock.dataBytes = 0;
        this->cmdData.numDataSets = packet[1];
        pos = 2;
    } else {
        pos = 0;
    }

    u8* const data = this->cmdData.attrBlock.data;
    while(pos < 16 && this->cmdData.numDataSets > 0) {
        for(; this->cmdData.attrBlock.dataBytes < 6 && pos < 16; this->cmdData.attrBlock.dataBytes++, pos++) {
            data[this->cmdData.attrBlock.dataBytes] = this->packet[pos];
        }

        if(this->cmdData.attrBlock.dataBytes == 6) {
            u8 pIn = (u8) (data[1] & 3);
            u8 pLine = (u8) ((data[1] >> 2) & 3);
            u8 pOut = (u8) ((data[1] >> 4) & 3);
            u8 x1 = data[2];
            u8 y1 = data[3];
            u8 x2 = data[4];
            u8 y2 = data[5];
            bool changeLine = (bool) (data[0] & 2);
            if(!changeLine) {
                if((data[0] & 7) == 1) {
                    changeLine = true;
                    pLine = pIn;
                } else if((data[0] & 7) == 4) {
                    changeLine = true;
                    pLine = pOut;
                }
            }

            if(data[0] & 1) { // Inside block
                for(int x = x1 + 1; x < x2; x++) {
                    for(int y = y1 + 1; y < y2; y++) {
                        this->map[y * 20 + x] = pIn;
                    }
                }
            }

            if(data[0] & 4) { // Outside block
                for(int x = 0; x < 20; x++) {
                    if(x < x1 || x > x2) {
                        for(int y = 0; y < 18; y++) {
                            if(y < y1 || y > y2) {
                                this->map[y * 20 + x] = pOut;
                            }
                        }
                    }
                }
            }

            if(changeLine) { // Line surrounding block
                for(int x = x1; x <= x2; x++) {
                    this->map[y1 * 20 + x] = pLine;
                    this->map[y2 * 20 + x] = pLine;
                }

                for(int y = y1; y <= y2; y++) {
                    this->map[y * 20 + x1] = pLine;
                    this->map[y * 20 + x2] = pLine;
                }
            }

            this-> cmdData.attrBlock.dataBytes = 0;
            this->cmdData.numDataSets--;
        }
    }
}

void SGB::attrLin(int block) {
    int index = 0;
    if(block == 0) {
        this->cmdData.numDataSets = packet[1];
        index = 2;
    }

    while(this->cmdData.numDataSets > 0 && index < 16) {
        u8 dat = packet[index++];
        this->cmdData.numDataSets--;

        u8 line = (u8) (dat & 0x1f);
        u8 pal = (u8) ((dat >> 5) & 3);

        if(dat & 0x80) { // Horizontal
            for(int i = 0; i < 20; i++) {
                this->map[line * 20 + i] = pal;
            }
        } else { // Vertical
            for(int i = 0; i < 18; i++) {
                this->map[i * 20 + line] = pal;
            }
        }
    }
}

void SGB::attrDiv(int block) {
    u8 p0 = (u8) ((this->packet[1] >> 2) & 3);
    u8 p1 = (u8) ((this->packet[1] >> 4) & 3);
    u8 p2 = (u8) ((this->packet[1] >> 0) & 3);

    if(this->packet[1] & 0x40) {
        for(int y = 0; y < this->packet[2] && y < 18; y++) {
            for(int x = 0; x < 20; x++) {
                this->map[y * 20 + x] = p0;
            }
        }

        if(this->packet[2] < 18) {
            for(int x = 0; x < 20; x++) {
                this->map[this->packet[2] * 20 + x] = p1;
            }

            for(int y = this->packet[2] + 1; y < 18; y++) {
                for(int x = 0; x < 20; x++) {
                    this->map[y * 20 + x] = p2;
                }
            }
        }
    } else {
        for(int x = 0; x < this->packet[2] && x < 20; x++) {
            for(int y = 0; y < 18; y++) {
                this->map[y * 20 + x] = p0;
            }
        }

        if(this->packet[2] < 20) {
            for(int y = 0; y < 18; y++) {
                this->map[y * 20 + this->packet[2]] = p1;
            }

            for(int x = packet[2] + 1; x < 20; x++) {
                for(int y = 0; y < 18; y++) {
                    this->map[y * 20 + x] = p2;
                }
            }
        }
    }
}

void SGB::attrChr(int block) {
    u8 &x = this->cmdData.attrChr.x;
    u8 &y = this->cmdData.attrChr.y;

    int index = 0;
    if(block == 0) {
        this->cmdData.numDataSets = this->packet[3] | (this->packet[4] << 8);
        this->cmdData.attrChr.writeStyle = (u8) (this->packet[5] & 1);
        x = (u8) (this->packet[1] >= 20 ? 19 : this->packet[1]);
        y = (u8) (this->packet[2] >= 18 ? 17 : this->packet[2]);

        index = 6 * 4;
    }

    while(this->cmdData.numDataSets != 0 && index < 16 * 4) {
        this->map[x + y * 20] = (u8) ((packet[index / 4] >> (6 - (index & 3) * 2)) & 3);
        if(this->cmdData.attrChr.writeStyle == 0) {
            x++;
            if(x == 20) {
                x = 0;
                y++;
                if(y == 18) {
                    y = 0;
                }
            }
        } else {
            y++;
            if(y == 18) {
                y = 0;
                x++;
                if(x == 20) {
                    x = 0;
                }
            }
        }

        index++;
        this->cmdData.numDataSets--;
    }
}

void SGB::sound(int block) {
    // Unimplemented
}

void SGB::souTrn(int block) {
    // Unimplemented
}

void SGB::palSet(int block) {
    for(int i = 0; i < 4; i++) {
        int paletteId = (this->packet[i * 2 + 1] | (this->packet[i * 2 + 2] << 8)) & 0x1ff;
        memcpy(this->gameboy->ppu->getBgPaletteData() + i * 4 + 1, sgbPalettes + paletteId * 8 + 2, 3 * sizeof(u16));
        memcpy(this->gameboy->ppu->getSprPaletteData() + i * 4 + 1, sgbPalettes + paletteId * 8 + 2, 3 * sizeof(u16));
        memcpy(this->gameboy->ppu->getSprPaletteData() + (i + 4) * 4 + 1, sgbPalettes + paletteId * 8 + 2, 3 * sizeof(u16));
    }

    int color0PaletteId = (this->packet[1] | (this->packet[2] << 8)) & 0x1ff;

    u16 color0 = (u16) (sgbPalettes[color0PaletteId * 8] | (sgbPalettes[color0PaletteId * 8 + 1] << 8));
    for(int i = 0; i < 4; i++) {
        this->gameboy->ppu->getBgPaletteData()[i * 4] = color0;
        this->gameboy->ppu->getSprPaletteData()[i * 4] = color0;
        this->gameboy->ppu->getSprPaletteData()[(i + 4) * 4] = color0;
    }

    if(this->packet[9] & 0x80) {
        this->loadAttrFile(this->packet[9] & 0x3f);
    }

    if(this->packet[9] & 0x40) {
        this->mask = 0;
    }
}

void SGB::palTrn(int block) {
    this->doVramTransfer(sgbPalettes);
}

void SGB::atrcEn(int block) {
    // Unimplemented
}

void SGB::testEn(int block) {
    // Unimplemented
}

void SGB::iconEn(int block) {
    // Unimplemented
}

void SGB::dataSnd(int block) {
    // Unimplemented
}

void SGB::dataTrn(int block) {
    // Unimplemented
}

void SGB::mltReq(int block) {
    this->numControllers = (u8) ((this->packet[1] & 3) + 1);
    this->selectedController = (u8) (this->numControllers > 1 ? 1 : 0);
}

void SGB::jump(int block) {
    // Unimplemented
}

void SGB::chrTrn(int block) {
    u8* data = new u8[0x1000];
    this->doVramTransfer(data);
    this->gameboy->ppu->setSgbTiles(data, this->packet[1]);
    delete data;
}

void SGB::pctTrn(int block) {
    u8* data = new u8[0x1000];
    this->doVramTransfer(data);
    this->gameboy->ppu->setSgbMap(data);
    delete data;
}

void SGB::attrTrn(int block) {
    this->doVramTransfer(sgbAttrFiles);
}

void SGB::attrSet(int block) {
    this->loadAttrFile(this->packet[1] & 0x3f);
    if(this->packet[1] & 0x40) {
        this->mask = 0;
    }
}

void SGB::maskEn(int block) {
    this->mask = (u8) (this->packet[1] & 3);
}

void SGB::objTrn(int block) {
    // Unimplemented
}

// End commands
