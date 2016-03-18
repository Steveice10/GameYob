#include <string.h>
#include <platform/system.h>

#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/gfx.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "ppu.h"
#include "sgb.h"
#include "romfile.h"

#define PALETTE_NUMBER (0x7)
#define VRAM_BANK (0x8)
#define SPRITE_NON_CGB_PALETTE_NUMBER (0x10)
#define FLIP_X (0x20)
#define FLIP_Y (0x40)
#define PRIORITY (0x80)

enum {
    LCD_HBLANK = 0,
    LCD_VBLANK = 1,
    LCD_ACCESS_OAM = 2,
    LCD_ACCESS_VRAM = 3
};

static const int modeCycles[] = {
        204,
        456,
        80,
        172
};

static const u8 depthOffset[8] = {
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
};

static const u32 BitStretchTable256[] = {
        0x0000, 0x0001, 0x0004, 0x0005, 0x0010, 0x0011, 0x0014, 0x0015, 0x0040, 0x0041, 0x0044, 0x0045, 0x0050, 0x0051, 0x0054, 0x0055,
        0x0100, 0x0101, 0x0104, 0x0105, 0x0110, 0x0111, 0x0114, 0x0115, 0x0140, 0x0141, 0x0144, 0x0145, 0x0150, 0x0151, 0x0154, 0x0155,
        0x0400, 0x0401, 0x0404, 0x0405, 0x0410, 0x0411, 0x0414, 0x0415, 0x0440, 0x0441, 0x0444, 0x0445, 0x0450, 0x0451, 0x0454, 0x0455,
        0x0500, 0x0501, 0x0504, 0x0505, 0x0510, 0x0511, 0x0514, 0x0515, 0x0540, 0x0541, 0x0544, 0x0545, 0x0550, 0x0551, 0x0554, 0x0555,
        0x1000, 0x1001, 0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015, 0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 0x1054, 0x1055,
        0x1100, 0x1101, 0x1104, 0x1105, 0x1110, 0x1111, 0x1114, 0x1115, 0x1140, 0x1141, 0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155,
        0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411, 0x1414, 0x1415, 0x1440, 0x1441, 0x1444, 0x1445, 0x1450, 0x1451, 0x1454, 0x1455,
        0x1500, 0x1501, 0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515, 0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551, 0x1554, 0x1555,
        0x4000, 0x4001, 0x4004, 0x4005, 0x4010, 0x4011, 0x4014, 0x4015, 0x4040, 0x4041, 0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055,
        0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 0x4114, 0x4115, 0x4140, 0x4141, 0x4144, 0x4145, 0x4150, 0x4151, 0x4154, 0x4155,
        0x4400, 0x4401, 0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415, 0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 0x4454, 0x4455,
        0x4500, 0x4501, 0x4504, 0x4505, 0x4510, 0x4511, 0x4514, 0x4515, 0x4540, 0x4541, 0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555,
        0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011, 0x5014, 0x5015, 0x5040, 0x5041, 0x5044, 0x5045, 0x5050, 0x5051, 0x5054, 0x5055,
        0x5100, 0x5101, 0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115, 0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 0x5154, 0x5155,
        0x5400, 0x5401, 0x5404, 0x5405, 0x5410, 0x5411, 0x5414, 0x5415, 0x5440, 0x5441, 0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455,
        0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 0x5514, 0x5515, 0x5540, 0x5541, 0x5544, 0x5545, 0x5550, 0x5551, 0x5554, 0x5555
};

static u16 RGBA5551ReverseTable[UINT16_MAX + 1];
static bool tablesInit = false;

void initTables() {
    if(!tablesInit) {
        for(u32 rgba5551 = 0; rgba5551 <= UINT16_MAX; rgba5551++) {
            RGBA5551ReverseTable[rgba5551] = (u16) ((rgba5551 & 0x1F) << 11 | ((rgba5551 >> 5) & 0x1F) << 6 | ((rgba5551 >> 10) & 0x1F) << 1 | 1);
        }

        tablesInit = true;
    }
}

PPU::PPU(Gameboy* gb) {
    this->gameboy = gb;
}

void PPU::reset() {
    initTables();

    this->lastScanlineCycle = 0;
    this->lastPhaseCycle = 0;
    this->halfSpeed = false;

    memset(this->vram[0], 0, 0x2000);
    memset(this->vram[1], 0, 0x2000);
    memset(this->oam, 0, 0xA0);

    memset(this->bgPaletteData, 0xFF, sizeof(this->bgPaletteData));
    memset(this->sprPaletteData, 0x00, sizeof(this->sprPaletteData));

    memset(this->tiles, 0, sizeof(this->tiles));

    this->gameboy->mmu->writeIO(LCDC, 0x91);
    this->gameboy->mmu->writeIO(BGP, 0xFC);
    this->gameboy->mmu->writeIO(OBP0, 0xFF);
    this->gameboy->mmu->writeIO(OBP1, 0xFF);
    this->gameboy->mmu->writeIO(HDMA5, 0xFF);

    this->mapBanks();

    this->gameboy->mmu->mapIOReadFunc(BCPD, [this](u16 addr) -> u8 {
        if(this->gameboy->gbMode == MODE_CGB) {
            return ((u8*) this->bgPaletteData)[this->gameboy->mmu->readIO(BCPS) & 0x3F];
        } else {
            return this->gameboy->mmu->readIO(BCPD);
        }
    });

    this->gameboy->mmu->mapIOReadFunc(OCPD, [this](u16 addr) -> u8 {
        if(this->gameboy->gbMode == MODE_CGB) {
            return ((u8*) this->sprPaletteData)[this->gameboy->mmu->readIO(OCPS) & 0x3F];
        } else {
            return this->gameboy->mmu->readIO(BCPD);
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(LCDC, [this](u16 addr, u8 val) -> void {
        u8 curr = this->gameboy->mmu->readIO(LCDC);
        this->gameboy->mmu->writeIO(LCDC, val);

        if((curr & 0x80) && !(val & 0x80)) {
            gfxClearScreenBuffer(0xFFFF);

            this->gameboy->mmu->writeIO(LY, 0);
            this->gameboy->mmu->writeIO(STAT, (u8) ((this->gameboy->mmu->readIO(STAT) & ~3) | LCD_HBLANK));
        } else if(!(curr & 0x80) && (val & 0x80)) {
            this->gameboy->mmu->writeIO(LY, 0);
            this->gameboy->mmu->writeIO(STAT, (u8) ((this->gameboy->mmu->readIO(STAT) & ~3) | LCD_ACCESS_OAM));

            this->lastScanlineCycle = this->gameboy->cpu->getCycle() - (4 << this->halfSpeed);
            this->gameboy->cpu->setEventCycle(this->lastScanlineCycle + modeCycles[LCD_ACCESS_OAM]);
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(STAT, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(STAT, (u8) ((this->gameboy->mmu->readIO(STAT) & 0x7) | (val & 0xF8)));
    });

    this->gameboy->mmu->mapIOWriteFunc(LY, [this](u16 addr, u8 val) -> void {
    });

    this->gameboy->mmu->mapIOWriteFunc(LYC, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(LYC, val);
        this->checkLYC();
    });

    this->gameboy->mmu->mapIOWriteFunc(DMA, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(DMA, val);

        u16 src = val << 8;
        for(u8 i = 0; i < 0xA0; i++) {
            this->oam[i] = this->gameboy->mmu->read(src + i);
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(VBK, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            this->gameboy->mmu->writeIO(VBK, (u8) (val | 0xFE));
            this->mapBanks();
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(BCPS, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            this->gameboy->mmu->writeIO(BCPS, (u8) (val | 0x40));
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(BCPD, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            u8 bcps = this->gameboy->mmu->readIO(BCPS);
            ((u8*) this->bgPaletteData)[bcps & 0x3F] = val;
            if(bcps & 0x80) {
                this->gameboy->mmu->writeIO(BCPS, (u8) (((bcps & 0x3F) + 1) | (bcps & 0x80) | 0x40));
            }
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(OCPS, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            this->gameboy->mmu->writeIO(OCPS, (u8) (val | 0x40));
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(OCPD, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            u8 ocps = this->gameboy->mmu->readIO(OCPS);
            ((u8*) this->sprPaletteData)[ocps & 0x3F] = val;
            if(ocps & 0x80) {
                this->gameboy->mmu->writeIO(OCPS, (u8) (((ocps & 0x3F) + 1) | (ocps & 0x80) | 0x40));
            }
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(HDMA5, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            if((this->gameboy->mmu->readIO(HDMA5) & 0x80) == 0) {
                if((val & 0x80) == 0) {
                    this->gameboy->mmu->writeIO(HDMA5, (u8) (val | 0x80));
                }
            } else {
                if(((val >> 7) & 1) == 0) {
                    u16 src = (u16) ((this->gameboy->mmu->readIO(HDMA2) | (this->gameboy->mmu->readIO(HDMA1) << 8)) & 0xFFF0);
                    u16 dst = (u16) ((this->gameboy->mmu->readIO(HDMA4) | (this->gameboy->mmu->readIO(HDMA3) << 8)) & 0x1FF0);
                    u8 length = (u8) ((val & 0x7F) + 1);
                    for(u8 i = 0; i < length; i++) {
                        for(u8 j = 0; j < 0x10; j++) {
                            this->gameboy->mmu->write((u16) (0x8000 + dst++), this->gameboy->mmu->read(src++));
                        }

                        dst &= 0x1FF0;
                    }

                    this->gameboy->mmu->writeIO(HDMA1, (u8) ((src >> 8) & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA2, (u8) (src & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA3, (u8) ((dst >> 8) & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA4, (u8) (dst & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA5, 0xFF);

                    this->gameboy->cpu->advanceCycles((u64) ((length * 8) << this->halfSpeed));
                } else {
                    this->gameboy->mmu->writeIO(HDMA5, (u8) (val & 0x7F));
                }
            }
        }
    });
}

void PPU::loadState(FILE* file, int version) {
    fread(&this->lastScanlineCycle, 1, sizeof(this->lastScanlineCycle), file);
    fread(&this->lastPhaseCycle, 1, sizeof(this->lastPhaseCycle), file);
    fread(&this->halfSpeed, 1, sizeof(this->halfSpeed), file);
    fread(this->vram, 1, sizeof(this->vram), file);
    fread(this->oam, 1, sizeof(this->oam), file);
    fread(this->bgPaletteData, 1, sizeof(this->bgPaletteData), file);
    fread(this->sprPaletteData, 1, sizeof(this->sprPaletteData), file);
    fread(this->tiles, 1, sizeof(this->tiles), file);

    this->mapBanks();
}

void PPU::saveState(FILE* file) {
    fwrite(&this->lastScanlineCycle, 1, sizeof(this->lastScanlineCycle), file);
    fwrite(&this->lastPhaseCycle, 1, sizeof(this->lastPhaseCycle), file);
    fwrite(&this->halfSpeed, 1, sizeof(this->halfSpeed), file);
    fwrite(this->vram, 1, sizeof(this->vram), file);
    fwrite(this->oam, 1, sizeof(this->oam), file);
    fwrite(this->bgPaletteData, 1, sizeof(this->bgPaletteData), file);
    fwrite(this->sprPaletteData, 1, sizeof(this->sprPaletteData), file);
    fwrite(this->tiles, 1, sizeof(this->tiles), file);
}

void PPU::checkLYC() {
    u8 stat = this->gameboy->mmu->readIO(STAT);
    if(this->gameboy->mmu->readIO(LY) == this->gameboy->mmu->readIO(LYC)) {
        this->gameboy->mmu->writeIO(STAT, (u8) (stat | 4));
        if(stat & 0x40) {
            this->gameboy->cpu->requestInterrupt(INT_LCD);
        }
    } else {
        this->gameboy->mmu->writeIO(STAT, (u8) (stat & ~4));
    }
}

void PPU::update() {
    if((this->gameboy->mmu->readIO(LCDC) & 0x80) == 0) {
        this->lastScanlineCycle = this->gameboy->cpu->getCycle();

        this->gameboy->mmu->writeIO(LY, 0);
        this->gameboy->mmu->writeIO(STAT, (u8) (this->gameboy->mmu->readIO(STAT) & 0xF8));

        // Ensure that we continue to return execution to frontend every frame, despite the LCD being off.
        while(this->gameboy->cpu->getCycle() >= this->lastPhaseCycle + (CYCLES_PER_FRAME << this->halfSpeed)) {
            this->lastPhaseCycle += CYCLES_PER_FRAME << this->halfSpeed;

            this->gameboy->frontendEvents |= RET_VBLANK;
        }

        this->gameboy->cpu->setEventCycle(this->lastPhaseCycle + (CYCLES_PER_FRAME << this->halfSpeed));
    } else {
        this->lastPhaseCycle = this->gameboy->cpu->getCycle();

        while(this->gameboy->cpu->getCycle() >= this->lastScanlineCycle + (modeCycles[this->gameboy->mmu->readIO(STAT) & 3] << this->halfSpeed)) {
            u8 stat = this->gameboy->mmu->readIO(STAT);
            u8 ly = this->gameboy->mmu->readIO(LY);
            u8 hdma5 = this->gameboy->mmu->readIO(HDMA5);

            this->lastScanlineCycle += modeCycles[stat & 3] << this->halfSpeed;

            switch(stat & 3) {
                case LCD_HBLANK:
                    this->gameboy->mmu->writeIO(LY, (u8) (ly + 1));
                    this->checkLYC();

                    if(ly + 1 >= 144) {
                        this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_VBLANK));

                        this->gameboy->cpu->requestInterrupt(INT_VBLANK);
                        if(stat & 0x10) {
                            this->gameboy->cpu->requestInterrupt(INT_LCD);
                        }

                        this->gameboy->frontendEvents |= RET_VBLANK;
                    } else {
                        this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_ACCESS_OAM));

                        if(stat & 0x20) {
                            this->gameboy->cpu->requestInterrupt(INT_LCD);
                        }
                    }

                    break;
                case LCD_VBLANK:
                    if(ly == 0) {
                        this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_ACCESS_OAM));

                        if(stat & 0x20) {
                            this->gameboy->cpu->requestInterrupt(INT_LCD);
                        }
                    } else {
                        this->gameboy->mmu->writeIO(LY, (u8) (ly + 1));
                        this->checkLYC();

                        if(ly >= 153) {
                            // Don't change the mode. Scanline 0 is twice as
                            // long as normal - half of it identifies as being
                            // in the vblank period.
                            this->gameboy->mmu->writeIO(LY, 0);
                            this->checkLYC();
                        }
                    }

                    break;
                case LCD_ACCESS_OAM:
                    this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_ACCESS_VRAM));
                    break;
                case LCD_ACCESS_VRAM:
                    if((!gfxGetFastForward() || fastForwardCounter >= fastForwardFrameSkip) && !this->gameboy->romFile->isGBS()) {
                        this->drawScanline(ly);
                    }

                    this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_HBLANK));

                    if(stat & 0x8) {
                        this->gameboy->cpu->requestInterrupt(INT_LCD);
                    }

                    if(this->gameboy->gbMode == MODE_CGB && (hdma5 & 0x80) == 0) {
                        u16 src = (u16) ((this->gameboy->mmu->readIO(HDMA2) | (this->gameboy->mmu->readIO(HDMA1) << 8)) & 0xFFF0);
                        u16 dst = (u16) ((this->gameboy->mmu->readIO(HDMA4) | (this->gameboy->mmu->readIO(HDMA3) << 8)) & 0x1FF0);
                        for(u8 i = 0; i < 0x10; i++) {
                            this->gameboy->mmu->write((u16) (0x8000 + dst++), this->gameboy->mmu->read(src++));
                        }

                        dst &= 0x1FF0;

                        this->gameboy->mmu->writeIO(HDMA1, (u8) ((src >> 8) & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA2, (u8) (src & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA3, (u8) ((dst >> 8) & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA4, (u8) (dst & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA5, (u8) (hdma5 - 1));

                        this->gameboy->cpu->advanceCycles((u64) (8 << this->halfSpeed));
                    }

                    break;
                default:
                    break;
            }
        }

        this->gameboy->cpu->setEventCycle(this->lastScanlineCycle + (modeCycles[this->gameboy->mmu->readIO(STAT) & 3] << this->halfSpeed));
    }
}

void PPU::mapBanks() {
    u8 bank = (u8) (this->gameboy->gbMode == MODE_CGB && (this->gameboy->mmu->readIO(VBK) & 0x1) != 0);
    this->gameboy->mmu->mapBank(0x8, this->vram[bank] + 0x0000);
    this->gameboy->mmu->mapBank(0x9, this->vram[bank] + 0x1000);

    auto write = [this](u16 addr, u8 val) -> void {
        u8 currBank = (u8) (this->gameboy->gbMode == MODE_CGB && (this->gameboy->mmu->readIO(VBK) & 0x1) != 0);
        u16 offset = (u16) (addr & 0x1FFF);
        if(this->vram[currBank][offset] != val) {
            this->vram[currBank][offset] = val;

            if(addr >= 0x8000 && addr < 0x9800) {
                u16 lineBase = (u16) (offset & ~0x1);
                u16 tile = (u16) ((offset >> 4) & 0x1FF);
                u8 y = (u8) ((offset >> 1) & 0x7);

                u32 pxData = BitStretchTable256[this->vram[currBank][lineBase]] | (BitStretchTable256[this->vram[currBank][lineBase + 1]] << 1);

                u8* base = &this->tiles[currBank][tile][y * 8];
                u8 shift = 14;
                for(u8 x = 0; x < 8; x++, shift -= 2) {
                    base[x] = (u8) ((pxData >> shift) & 0x3);
                }
            }
        }
    };

    this->gameboy->mmu->mapBankWriteFunc(0x8, write);
    this->gameboy->mmu->mapBankWriteFunc(0x9, write);
}

void PPU::setHalfSpeed(bool halfSpeed) {
    if(!this->halfSpeed && halfSpeed) {
        this->lastScanlineCycle -= this->gameboy->cpu->getCycle() - this->lastScanlineCycle;
        this->lastPhaseCycle -= this->gameboy->cpu->getCycle() - this->lastPhaseCycle;
    } else if(this->halfSpeed && !halfSpeed) {
        this->lastScanlineCycle += (this->gameboy->cpu->getCycle() - this->lastScanlineCycle) / 2;
        this->lastPhaseCycle += (this->gameboy->cpu->getCycle() - this->lastPhaseCycle) / 2;
    }

    this->halfSpeed = halfSpeed;
}

void PPU::transferTiles(u8* dest) {
    u8 lcdc = this->gameboy->mmu->readIO(LCDC);

    u32 map = (u32) (0x1800 + ((lcdc >> 3) & 1) * 0x400);
    u32 index = 0;
    for(u32 y = 0; y < 18; y++) {
        for(u32 x = 0; x < 20; x++) {
            if(index == 0x1000) {
                return;
            }

            u8 tile = this->vram[0][map + y * 32 + x];
            if(lcdc & 0x10) {
                memcpy(dest + index, &this->vram[0][tile * 0x10], 0x10);
            } else {
                memcpy(dest + index, &this->vram[0][0x1000 + (s8) tile * 0x10], 0x10);
            }

            index += 0x10;
        }
    }
}

void PPU::drawScanline(u32 scanline) {
    switch(this->gameboy->sgb->getGfxMask()) {
        case 0: {
            u16* lineBuffer = gfxGetLineBuffer(scanline);
            u8 depthBuffer[256];

            this->drawBackground(lineBuffer, depthBuffer, scanline);
            this->drawWindow(lineBuffer, depthBuffer, scanline);
            this->drawSprites(lineBuffer, depthBuffer, scanline);
            break;
        }
        case 2:
            gfxClearLineBuffer(scanline, 0x0000);
            break;
        case 3:
            gfxClearLineBuffer(scanline, RGBA5551ReverseTable[this->gameboy->sgb->getActivePalette()[this->gameboy->mmu->readIO(BGP) & 3]]);
            break;
        default:
            break;
    }
}

void PPU::drawBackground(u16* lineBuffer, u8* depthBuffer, u32 scanline) {
    u8 lcdc = this->gameboy->mmu->readIO(LCDC);
    if(this->gameboy->gbMode == MODE_CGB || (lcdc & 0x01) != 0) { // Background enabled
        u8 scx = this->gameboy->mmu->readIO(SCX);
        u8 scy = this->gameboy->mmu->readIO(SCY);
        u8 wx = this->gameboy->mmu->readIO(WX);
        u8 wy = this->gameboy->mmu->readIO(WY);
        u8 bgp = this->gameboy->mmu->readIO(BGP);

        u8* sgbMap = this->gameboy->sgb->getGfxMap();
        u16* palette = this->gameboy->gbMode == MODE_CGB ? (u16*) this->bgPaletteData : this->gameboy->gbMode == MODE_SGB ? this->gameboy->sgb->getActivePalette() : gbBgPalette;

        bool tileSigned = (lcdc & 0x10) == 0;
        bool losePriority = this->gameboy->gbMode == MODE_CGB && (lcdc & 0x01) == 0;

        // The y position (measured in tiles)
        u32 tileY = ((scanline + scy) & 0xFF) >> 3;
        u32 basePixelY = (scanline + scy) & 0x07;

        // Tile Map address plus row offset
        u32 bgMapAddr = 0x1800 + ((lcdc >> 3) & 1) * 0x400 + (tileY * 32);

        // Number of tiles to draw in a row
        u32 numTilesX = 20;
        if(wy <= scanline && wy < 144 && wx >= 7 && wx < 167 && (lcdc & 0x20) != 0) {
            numTilesX = wx >> 3;
        }

        // Tiles to draw
        u32 startTile = scx >> 3;
        u32 endTile = (startTile + numTilesX + 1) & 31;

        // Calculate lineBuffer Start, negatives treated as unsigned for speed up
        u8 writeX = (u8) (-(scx & 0x07));
        for(u32 tile = startTile; tile != endTile; tile = (tile + 1) & 31) {
            // The address, from the beginning of VRAM, of the tile's mapping
            u32 mapAddr = bgMapAddr + tile;

            // This is the tile id.
            u32 tileNum = this->vram[0][mapAddr];
            if(tileSigned) {
                tileNum = ((s8) tileNum) + 0x100;
            }

            u32 pixelY = basePixelY;
            u8 depth = 1;
            u32 paletteId = 0;
            u32 bank = 0;

            int start = 0;
            int end = 8;
            int change = 1;

            // Setup Tile Info
            if(this->gameboy->gbMode == MODE_CGB) {
                u32 flag = this->vram[1][mapAddr];
                paletteId = flag & PALETTE_NUMBER;
                bank = (u32) ((flag & VRAM_BANK) != 0);

                if(flag & FLIP_X) {
                    start = 7;
                    end = -1;
                    change = -1;
                }

                if(flag & FLIP_Y) {
                    pixelY = 7 - pixelY;
                }

                if(losePriority) {
                    depth = 0;
                } else if(flag & PRIORITY) {
                    depth = 3;
                }
            }

            u8* tileLine = &this->tiles[bank][tileNum][pixelY * 8];
            u8* subSgbMap = &sgbMap[(scanline >> 3) * 20];
            for(int x = start; x != end; x += change, writeX++) {
                if(writeX >= 160) {
                    continue;
                }

                u32 colorId = tileLine[x];
                u32 subPaletteId = this->gameboy->gbMode == MODE_SGB ? subSgbMap[writeX >> 3] : paletteId;
                u8 paletteIndex = this->gameboy->gbMode != MODE_CGB ? (u8) ((bgp >> (colorId * 2)) & 3) : (u8) colorId;

                // Draw pixel
                depthBuffer[writeX] = depth - depthOffset[losePriority * 4 + colorId];
                lineBuffer[writeX] = RGBA5551ReverseTable[palette[subPaletteId * 4 + paletteIndex]];
            }
        }
    }
}

void PPU::drawWindow(u16* lineBuffer, u8* depthBuffer, u32 scanline) {
    u8 lcdc = this->gameboy->mmu->readIO(LCDC);
    u8 wx = this->gameboy->mmu->readIO(WX);
    u8 wy = this->gameboy->mmu->readIO(WY);
    if(wy <= scanline && wy < 144 && wx >= 7 && wx < 167 && (lcdc & 0x20) != 0) { // Window enabled
        u8 bgp = this->gameboy->mmu->readIO(BGP);

        u8* sgbMap = this->gameboy->sgb->getGfxMap();
        u16* palette = this->gameboy->gbMode == MODE_CGB ? (u16*) this->bgPaletteData : this->gameboy->gbMode == MODE_SGB ? this->gameboy->sgb->getActivePalette() : gbBgPalette;

        bool tileSigned = (lcdc & 0x10) == 0;
        bool losePriority = this->gameboy->gbMode == MODE_CGB && (lcdc & 0x01) == 0;

        // The y position (measured in tiles)
        u32 tileY = (scanline - wy) >> 3;
        u32 basePixelY = (scanline - wy) & 0x07;

        // Tile Map address plus row offset
        u32 winMapAddr = 0x1800 + ((lcdc >> 6) & 1) * 0x400 + (tileY * 32);

        // Tiles to draw
        u32 endTile = (u32) (21 - ((wx - 7) >> 3));

        // Calculate lineBuffer Start, negatives treated as unsigned for speed up
        u32 writeX = (u32) (wx - 7);
        for(u32 tile = 0; tile < endTile; tile++) {
            // The address, from the beginning of VRAM, of the tile's mapping
            u32 mapAddr = winMapAddr + tile;

            // This is the tile id.
            u32 tileNum = this->vram[0][mapAddr];
            if(tileSigned) {
                tileNum = ((s8) tileNum) + 0x100;
            }

            u32 pixelY = basePixelY;
            u8 depth = 1;
            u32 paletteId = 0;
            u32 bank = 0;

            int start = 0;
            int end = 8;
            int change = 1;

            // Setup Tile Info
            if(this->gameboy->gbMode == MODE_CGB) {
                u32 flag = this->vram[1][mapAddr];
                paletteId = flag & PALETTE_NUMBER;
                bank = (u32) ((flag & VRAM_BANK) != 0);

                if(flag & FLIP_X) {
                    start = 7;
                    end = -1;
                    change = -1;
                }

                if(flag & FLIP_Y) {
                    pixelY = 7 - pixelY;
                }

                if(losePriority) {
                    depth = 0;
                } else if(flag & PRIORITY) {
                    depth = 3;
                }
            }

            u8* tileLine = &this->tiles[bank][tileNum][pixelY * 8];
            u8* subSgbMap = &sgbMap[(scanline >> 3) * 20];
            for(int x = start; x != end; x += change, writeX++) {
                if(writeX >= 160) {
                    continue;
                }

                u32 colorId = tileLine[x];
                u32 subPaletteId = this->gameboy->gbMode == MODE_SGB ? subSgbMap[writeX >> 3] : paletteId;
                u8 paletteIndex = this->gameboy->gbMode != MODE_CGB ? (u8) ((bgp >> (colorId * 2)) & 3) : (u8) colorId;

                // Draw pixel
                depthBuffer[writeX] = depth - depthOffset[losePriority * 4 + colorId];
                lineBuffer[writeX] = RGBA5551ReverseTable[palette[subPaletteId * 4 + paletteIndex]];
            }
        }
    }
}

void PPU::drawSprites(u16* lineBuffer, u8* depthBuffer, u32 scanline) {
    u8 lcdc = this->gameboy->mmu->readIO(LCDC);
    if(lcdc & 0x2) { // Sprites enabled
        u8* sgbMap = this->gameboy->sgb->getGfxMap();
        u16* palette = this->gameboy->gbMode == MODE_CGB ? (u16*) this->sprPaletteData : this->gameboy->gbMode == MODE_SGB ? this->gameboy->sgb->getActivePalette() : gbSprPalette;

        u32 height = (lcdc & 0x4) ? 16 : 8;

        for(u32 sprite = 39; (int) sprite >= 0; sprite--) {
            // The sprite's number, times 4 (each uses 4 bytes)
            u32 spriteOffset = sprite * 4;

            u32 sy = this->oam[spriteOffset];
            u32 sx = this->oam[spriteOffset + 1];
            u32 tileNum = this->oam[spriteOffset + 2];
            u32 flag = this->oam[spriteOffset + 3];

            // Clip Sprite to or bottom
            if(scanline + 16 < sy || scanline + 16 >= sy + height) {
                continue;
            }

            // Setup Tile Info
            u32 bank = 0;
            u32 paletteId = 0;
            u8 obp = 0;

            if(this->gameboy->gbMode == MODE_CGB) {
                bank = (flag & VRAM_BANK) >> 3;
                paletteId = flag & PALETTE_NUMBER;
            } else {
                paletteId = (flag & SPRITE_NON_CGB_PALETTE_NUMBER) >> 2;
                obp = this->gameboy->mmu->readIO((u16) (OBP0 + paletteId / 4));
            }

            // Select tile base on tile Y offset
            if(height == 16) {
                tileNum &= ~1;
                if(scanline - sy + 16 >= 8) {
                    tileNum++;
                }
            }

            // This is the tile's Y position to be read (0-7)
            u32 pixelY = (scanline - sy + 16) & 0x07;
            if(flag & FLIP_Y) {
                pixelY = 7 - pixelY;
                if(height == 16) {
                    tileNum = tileNum ^ 1;
                }
            }

            int start = 0;
            int end = 8;
            int change = 1;

            // Reverse their bits if flipX set
            if(flag & FLIP_X) {
                start = 7;
                end = -1;
                change = -1;
            }

            // Setup depth based on priority
            u8 depth = 2;
            if(flag & PRIORITY) {
                depth = 0;
            }

            // Calculate where to start to draw, negatives treated as unsigned for speed up
            u32 writeX = (u32) ((s32) (sx - 8));

            u8* tileLine = &this->tiles[bank][tileNum][pixelY * 8];
            u8* subSgbMap = &sgbMap[(scanline >> 3) * 20];
            for(int x = start; x != end; x += change, writeX++) {
                if(writeX >= 160) {
                    continue;
                }

                u32 colorId = tileLine[x];

                // Draw pixel if not transparent or above depth buffer
                if(colorId != 0 && depth >= depthBuffer[writeX]) {
                    u32 subPaletteId = paletteId + subSgbMap[writeX >> 3];
                    u8 paletteIndex = this->gameboy->gbMode != MODE_CGB ? (u8) ((obp >> (colorId * 2)) & 3) : (u8) colorId;

                    depthBuffer[writeX] = depth;
                    lineBuffer[writeX] = RGBA5551ReverseTable[palette[subPaletteId * 4 + paletteIndex]];
                }
            }
        }
    }
}
