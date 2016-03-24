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
        0x01, 0x00, 0x00, 0x00
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

    if(this->gameboy->gbMode == MODE_CGB) {
        for(u8 i = 0; i < sizeof(this->expandedBgp); i++) {
            this->expandedBgp[i] = i;
        }

        for(u8 i = 0; i < sizeof(this->expandedObp); i++) {
            this->expandedObp[i] = (u8) (i & 3);
        }
    } else {
        memset(this->expandedBgp, 3, sizeof(this->expandedBgp));
        this->expandedBgp[0] = 0;
        memset(this->expandedObp, 3, sizeof(this->expandedObp));
    }

    memset(this->bgPaletteData, 0xFF, sizeof(this->bgPaletteData));
    memset(this->sprPaletteData, 0x00, sizeof(this->sprPaletteData));

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

    this->gameboy->mmu->mapIOWriteFunc(BGP, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(BGP, val);

        if(this->gameboy->gbMode != MODE_CGB) {
            for(u8 i = 0; i < 4; i++) {
                this->expandedBgp[i] = (u8) ((val >> (i * 2)) & 3);
            }
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(OBP0, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(OBP0, val);

        if(this->gameboy->gbMode != MODE_CGB) {
            for(u8 i = 0; i < 4; i++) {
                this->expandedObp[i] = (u8) ((val >> (i * 2)) & 3);
            }
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(OBP1, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(OBP1, val);

        if(this->gameboy->gbMode != MODE_CGB) {
            for(u8 i = 0; i < 4; i++) {
                this->expandedObp[4 + i] = (u8) ((val >> (i * 2)) & 3);
            }
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
                    u8 bank = (u8) (this->gameboy->gbMode == MODE_CGB && (this->gameboy->mmu->readIO(VBK) & 0x1) != 0);
                    u16 src = (u16) ((this->gameboy->mmu->readIO(HDMA2) | (this->gameboy->mmu->readIO(HDMA1) << 8)) & 0xFFF0);
                    u16 dst = (u16) ((this->gameboy->mmu->readIO(HDMA4) | (this->gameboy->mmu->readIO(HDMA3) << 8)) & 0x1FF0);
                    u8 length = (u8) ((val & 0x7F) + 1);
                    for(u8 i = 0; i < length; i++) {
                        for(u8 j = 0; j < 0x10; j++) {
                            this->vram[bank][dst++] = this->gameboy->mmu->read(src++);
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

void PPU::loadState(std::istream& data, u8 version) {
    data.read((char*) &this->lastScanlineCycle, sizeof(this->lastScanlineCycle));
    data.read((char*) &this->lastPhaseCycle, sizeof(this->lastPhaseCycle));
    data.read((char*) &this->halfSpeed, sizeof(this->halfSpeed));
    data.read((char*) this->vram, sizeof(this->vram));
    data.read((char*) this->oam, sizeof(this->oam));
    data.read((char*) this->expandedBgp, sizeof(this->expandedBgp));
    data.read((char*) this->expandedObp, sizeof(this->expandedObp));
    data.read((char*) this->bgPaletteData, sizeof(this->bgPaletteData));
    data.read((char*) this->sprPaletteData, sizeof(this->sprPaletteData));

    this->mapBanks();
}

void PPU::saveState(std::ostream& data) {
    data.write((char*) &this->lastScanlineCycle, sizeof(this->lastScanlineCycle));
    data.write((char*) &this->lastPhaseCycle, sizeof(this->lastPhaseCycle));
    data.write((char*) &this->halfSpeed, sizeof(this->halfSpeed));
    data.write((char*) this->vram, sizeof(this->vram));
    data.write((char*) this->oam, sizeof(this->oam));
    data.write((char*) this->expandedBgp, sizeof(this->expandedBgp));
    data.write((char*) this->expandedObp, sizeof(this->expandedObp));
    data.write((char*) this->bgPaletteData, sizeof(this->bgPaletteData));
    data.write((char*) this->sprPaletteData, sizeof(this->sprPaletteData));
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
                    ly++;
                    this->gameboy->mmu->writeIO(LY, ly);
                    this->checkLYC();

                    if(ly >= 144) {
                        this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_VBLANK));

                        this->gameboy->cpu->requestInterrupt(INT_VBLANK);
                        if(stat & 0x10) {
                            this->gameboy->cpu->requestInterrupt(INT_LCD);
                        } else if(stat & 0x20) {
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
                        ly++;
                        this->gameboy->mmu->writeIO(LY, ly);
                        this->checkLYC();

                        if(ly >= 153) {
                            // Don't change the mode. Scanline 0 is twice as
                            // long as normal - half of it identifies as being
                            // in the vblank period.
                            ly = 0;
                            this->gameboy->mmu->writeIO(LY, ly);
                            this->checkLYC();
                        }
                    }

                    break;
                case LCD_ACCESS_OAM:
                    this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_ACCESS_VRAM));
                    break;
                case LCD_ACCESS_VRAM:
                    if(!gfxGetFastForward() || fastForwardCounter >= fastForwardFrameSkip) {
                        this->drawScanline(ly);
                    }

                    this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_HBLANK));

                    if(stat & 0x8) {
                        this->gameboy->cpu->requestInterrupt(INT_LCD);
                    }

                    if(this->gameboy->gbMode == MODE_CGB && (hdma5 & 0x80) == 0) {
                        u8 bank = (u8) (this->gameboy->gbMode == MODE_CGB && (this->gameboy->mmu->readIO(VBK) & 0x1) != 0);
                        u16 src = (u16) ((this->gameboy->mmu->readIO(HDMA2) | (this->gameboy->mmu->readIO(HDMA1) << 8)) & 0xFFF0);
                        u16 dst = (u16) ((this->gameboy->mmu->readIO(HDMA4) | (this->gameboy->mmu->readIO(HDMA3) << 8)) & 0x1FF0);
                        for(u8 i = 0; i < 0x10; i++) {
                            this->vram[bank][dst++] = this->gameboy->mmu->read(src++);
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
            u8 lcdc = this->gameboy->mmu->readIO(LCDC);
            if((lcdc & 0x80) != 0) {
                u16* lineBuffer = gfxGetLineBuffer(scanline);
                u8 depthBuffer[160];

                // Background
                if(this->gameboy->gbMode == MODE_CGB || (lcdc & 0x01) != 0) {
                    u8* subSgbMap = &this->gameboy->sgb->getPaletteMap()[(scanline >> 3) * 20];
                    u16* basePalette = this->gameboy->gbMode == MODE_CGB ? (u16*) this->bgPaletteData : this->gameboy->gbMode == MODE_SGB ? this->gameboy->sgb->getActivePalette() : gbBgPalette;

                    u8 basePixelX = this->gameboy->mmu->readIO(SCX);
                    u8 baseTileX = basePixelX >> 3;
                    u8 baseSubTileX = (u8) (basePixelX & 7);

                    u8 pixelY = (u8) (scanline + this->gameboy->mmu->readIO(SCY));
                    u8 tileY = pixelY >> 3;
                    u8 subTileY = (u8) (pixelY & 7);

                    u16 lineMapOffset = (u16) (0x1800 + ((lcdc >> 3) & 1) * 0x400 + (tileY * 32));
                    u8* lineTileMap = &this->vram[0][lineMapOffset];
                    u8* lineFlagMap = &this->vram[1][lineMapOffset];

                    for(u8 tileX = 0; tileX < 21; tileX++) {
                        u8 mapTileX = (u8) ((baseTileX + tileX) & 31);
                        u16 tileId = (u16) ((lcdc & 0x10) != 0 ? lineTileMap[mapTileX] : (s8) lineTileMap[mapTileX] + 0x100);
                        u8 flags = (u8) (this->gameboy->gbMode == MODE_CGB ? lineFlagMap[mapTileX] : 0);

                        u8 paletteId = (u8) (flags & 7);
                        u8 bank = (u8) ((flags >> 3) & 1);
                        bool flipX = (bool) ((flags >> 5) & 1);
                        bool flipY = (bool) ((flags >> 6) & 1);
                        u8 depth = (u8) (1 + ((flags >> 6) & 2));

                        u8 paletteIds[3];
                        if(this->gameboy->gbMode == MODE_SGB) {
                            memcpy(paletteIds, &subSgbMap[tileX - 1], sizeof(paletteIds));
                        } else {
                            memset(paletteIds, paletteId, sizeof(paletteIds));
                        }

                        u16 offset = (u16) ((tileId * 0x10) + ((flipY ? 7 - subTileY : subTileY) * 2));
                        u16 pxData = (u16) (BitStretchTable256[this->vram[bank][offset]] | (BitStretchTable256[this->vram[bank][offset + 1]] << 1));

                        for(u8 x = 0; x < 8; x++) {
                            s16 pixelX = (s16) (tileX * 8 + x - baseSubTileX);
                            if(pixelX < 0 || pixelX >= 160) {
                                continue;
                            }

                            u8 colorId = (u8) ((pxData >> (flipX ? x * 2 : 14 - x * 2)) & 3);
                            lineBuffer[pixelX] = RGBA5551ReverseTable[basePalette[paletteIds[(pixelX >> 3) - tileX + 1] * 4 + this->expandedBgp[colorId]]];
                            depthBuffer[pixelX] = depth - depthOffset[colorId];
                        }
                    }

                    if(this->gameboy->gbMode == MODE_CGB && (lcdc & 0x01) == 0) {
                        memset(depthBuffer, 0, 160);
                    }
                }

                // Window
                if((lcdc & 0x20) != 0) {
                    u8 wx = this->gameboy->mmu->readIO(WX);
                    u8 wy = this->gameboy->mmu->readIO(WY);
                    if(wy <= scanline && wy < 144 && wx >= 0 && wx < 167) {
                        u8* subSgbMap = &this->gameboy->sgb->getPaletteMap()[(scanline >> 3) * 20];
                        u16* basePalette = this->gameboy->gbMode == MODE_CGB ? (u16*) this->bgPaletteData : this->gameboy->gbMode == MODE_SGB ? this->gameboy->sgb->getActivePalette() : gbBgPalette;

                        s16 basePixelX = (s16) (wx - 7);
                        s16 baseTileX = (s16) (basePixelX >> 3);

                        u8 pixelY = (u8) (scanline - wy);
                        u8 tileY = pixelY >> 3;
                        u8 subTileY = (u8) (pixelY & 7);

                        u16 lineMapOffset = (u16) (0x1800 + ((lcdc >> 6) & 1) * 0x400 + (tileY * 32));
                        u8* lineTileMap = &this->vram[0][lineMapOffset];
                        u8* lineFlagMap = &this->vram[1][lineMapOffset];

                        u8 tileCount = (u8) (20 - baseTileX);
                        for(u8 tileX = 0; tileX < tileCount; tileX++) {
                            u16 tileId = (u16) ((lcdc & 0x10) != 0 ? lineTileMap[tileX] : (s8) lineTileMap[tileX] + 0x100);
                            u8 flags = (u8) (this->gameboy->gbMode == MODE_CGB ? lineFlagMap[tileX] : 0);

                            u8 paletteId = (u8) (flags & 7);
                            u8 bank = (u8) ((flags >> 3) & 1);
                            bool flipX = (bool) ((flags >> 5) & 1);
                            bool flipY = (bool) ((flags >> 6) & 1);
                            u8 depth = (u8) (1 + ((flags >> 6) & 2));

                            u8 paletteIds[3];
                            if(this->gameboy->gbMode == MODE_SGB) {
                                memcpy(paletteIds, &subSgbMap[tileX - 1], sizeof(paletteIds));
                            } else {
                                memset(paletteIds, paletteId, sizeof(paletteIds));
                            }

                            u16 offset = (u16) ((tileId * 0x10) + ((flipY ? 7 - subTileY : subTileY) * 2));
                            u16 pxData = (u16) (BitStretchTable256[this->vram[bank][offset]] | (BitStretchTable256[this->vram[bank][offset + 1]] << 1));

                            for(u8 x = 0; x < 8; x++) {
                                s16 pixelX = (s16) (tileX * 8 + x + basePixelX);
                                if(pixelX < 0 || pixelX >= 160) {
                                    continue;
                                }

                                u8 colorId = (u8) ((pxData >> (flipX ? x * 2 : 14 - x * 2)) & 3);
                                lineBuffer[pixelX] = RGBA5551ReverseTable[basePalette[paletteIds[(pixelX >> 3) - tileX + 1] * 4 + this->expandedBgp[colorId]]];
                                depthBuffer[pixelX] = depth - depthOffset[colorId];
                            }
                        }

                        if(this->gameboy->gbMode == MODE_CGB && (lcdc & 0x01) == 0) {
                            memset(depthBuffer, 0, 160);
                        }
                    }
                }

                // Sprites
                if((lcdc & 0x02) != 0) {
                    u8* subSgbMap = &this->gameboy->sgb->getPaletteMap()[(scanline >> 3) * 20];
                    u16* basePalette = this->gameboy->gbMode == MODE_CGB ? (u16*) this->sprPaletteData : this->gameboy->gbMode == MODE_SGB ? this->gameboy->sgb->getActivePalette() : gbSprPalette;

                    u32 spriteHeight = (lcdc & 0x04) != 0 ? 16 : 8;

                    for(s8 sprite = 39; sprite >= 0; sprite--) {
                        u32 spriteY = this->oam[sprite * 4 + 0];

                        if(scanline + 16 < spriteY || scanline + 16 >= spriteY + spriteHeight) {
                            continue;
                        }

                        u32 spriteX = this->oam[sprite * 4 + 1];
                        u32 tileId = this->oam[sprite * 4 + 2];
                        u32 flags = this->oam[sprite * 4 + 3];

                        s16 basePixelX = (s16) (spriteX - 8);
                        s8 baseTileX = (s8) (basePixelX >> 3);

                        u8 pixelY = (u8) (scanline - spriteY + 16);
                        u8 subTileY = (u8) (pixelY & 7);

                        u8 paletteId = (u8) (flags & 7);
                        u8 bank = (u8) ((flags >> 3) & 1);
                        u8 obpId = (u8) ((flags >> 4) & 1);
                        bool flipX = (bool) ((flags >> 5) & 1);
                        bool flipY = (bool) ((flags >> 6) & 1);
                        u8 depth = (u8) (2 - ((flags >> 6) & 2));

                        if(spriteHeight == 16) {
                            tileId &= ~1;
                            if(pixelY >= 8) {
                                tileId++;
                            }

                            if(flipY) {
                                tileId = tileId ^ 1;
                            }
                        }

                        u8 paletteIds[2];
                        if(this->gameboy->gbMode == MODE_SGB) {
                            memcpy(paletteIds, &subSgbMap[baseTileX], sizeof(paletteIds));
                        } else {
                            memset(paletteIds, paletteId, sizeof(paletteIds));
                        }

                        u16 offset = (u16) ((tileId * 0x10) + ((flipY ? 7 - subTileY : subTileY) * 2));
                        u16 pxData = (u16) (BitStretchTable256[this->vram[bank][offset]] | (BitStretchTable256[this->vram[bank][offset + 1]] << 1));

                        for(u8 x = 0; x < 8; x++) {
                            s16 pixelX = (s16) (basePixelX + x);
                            if(pixelX < 0 || pixelX >= 160) {
                                continue;
                            }

                            u8 colorId = (u8) ((pxData >> (flipX ? x * 2 : 14 - x * 2)) & 3);
                            if(colorId != 0 && depth >= depthBuffer[pixelX]) {
                                lineBuffer[pixelX] = RGBA5551ReverseTable[basePalette[paletteIds[(pixelX >> 3) - baseTileX] * 4 + this->expandedObp[obpId * 4 + colorId]]];
                                depthBuffer[pixelX] = depth;
                            }
                        }
                    }
                }
            }

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