#include <string.h>

#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "ppu.h"
#include "sgb.h"

enum {
    LCD_HBLANK = 0,
    LCD_VBLANK = 1,
    LCD_ACCESS_OAM = 2,
    LCD_ACCESS_OAM_VRAM = 3
};

static u16 grayScalePalette[] = {
        0xFFFF, 0x5294, 0x294A, 0x0000,
        0xFFFF, 0x5294, 0x294A, 0x0000,
        0xFFFF, 0x5294, 0x294A, 0x0000,
        0xFFFF, 0x5294, 0x294A, 0x0000,
        0xFFFF, 0x5294, 0x294A, 0x0000,
        0xFFFF, 0x5294, 0x294A, 0x0000,
        0xFFFF, 0x5294, 0x294A, 0x0000,
        0xFFFF, 0x5294, 0x294A, 0x0000,
};

static const int modeCycles[] = {
        204,
        456,
        80,
        172
};

static const u8 depthOffset[] = {
        1, 0, 0, 0
};

static const u16 BitStretchTable256[] = {
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

static const u8 BitReverseTable256[] = {
        0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
        0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
        0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
        0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
        0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
        0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
        0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
        0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
        0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
        0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
        0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
        0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
        0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
        0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
        0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
        0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

static u32 RGB555ToRGBA8888Table[UINT16_MAX + 1];
static bool tablesInit = false;

static void initTables() {
    if(!tablesInit) {
        for(u32 rgba5551 = 0; rgba5551 <= UINT16_MAX; rgba5551++) {
            u8 r5 = (u8) ((rgba5551 >> 0) & 0x1F);
            u8 g5 = (u8) ((rgba5551 >> 5) & 0x1F);
            u8 b5 = (u8) ((rgba5551 >> 10) & 0x1F);

            u8 r8 = ((r5 << 3) | (r5 >> 2));
            u8 g8 = ((g5 << 3) | (g5 >> 2));
            u8 b8 = ((b5 << 3) | (b5 >> 2));

            RGB555ToRGBA8888Table[rgba5551] = (u32) (r8 << 24 | g8 << 16 | b8 << 8 | 0xFF);
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
    this->lastOamDmaBeginCycle = 0;
    this->halfSpeed = false;

    this->scanlineX = 0;

    this->oamDmaSrc = 0;

    memset(this->currTileLines, 0, sizeof(this->currTileLines));
    memset(this->currSpriteLines, 0, sizeof(this->currSpriteLines));
    this->currSprites = 0;

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

    memset(this->vram, 0, sizeof(this->vram));
    memset(this->oam, 0, sizeof(this->oam));

    if(this->gameboy->gbMode == MODE_CGB) {
        memset(this->bgPaletteData, 0xFF, sizeof(this->bgPaletteData));
        memset(this->sprPaletteData, 0x00, sizeof(this->sprPaletteData));
    } else {
        memcpy(this->bgPaletteData, grayScalePalette, sizeof(this->bgPaletteData));
        memcpy(this->sprPaletteData, grayScalePalette, sizeof(this->sprPaletteData));
    }

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
        this->oamDmaSrc = val << 8;
        this->lastOamDmaBeginCycle = this->gameboy->cpu->getCycle();
        this->gameboy->cpu->setEventCycle(this->lastOamDmaBeginCycle + 0xA0);
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
        if(this->gameboy->gbMode == MODE_CGB && (this->gameboy->mmu->readIO(STAT) & 3) != LCD_ACCESS_OAM_VRAM) {
            u8 bcps = this->gameboy->mmu->readIO(BCPS);
            ((u8*) this->bgPaletteData)[bcps & 0x3F] = val;
            if(bcps & 0x80) {
                this->gameboy->mmu->writeIO(BCPS, (u8) (((bcps & 0x3F) + 1) | (bcps & 0x80) | 0x40));
            }
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(OCPS, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB && (this->gameboy->mmu->readIO(STAT) & 3) != LCD_ACCESS_OAM_VRAM) {
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
    data.read((char*) &this->lastOamDmaBeginCycle, sizeof(this->lastOamDmaBeginCycle));
    data.read((char*) &this->halfSpeed, sizeof(this->halfSpeed));
    data.read((char*) &this->scanlineX, sizeof(this->scanlineX));
    data.read((char*) &this->oamDmaSrc, sizeof(this->oamDmaSrc));
    data.read((char*) this->expandedBgp, sizeof(this->expandedBgp));
    data.read((char*) this->expandedObp, sizeof(this->expandedObp));
    data.read((char*) this->vram, sizeof(this->vram));
    data.read((char*) this->oam, sizeof(this->oam));
    data.read((char*) this->bgPaletteData, sizeof(this->bgPaletteData));
    data.read((char*) this->sprPaletteData, sizeof(this->sprPaletteData));

    this->mapBanks();
}

void PPU::saveState(std::ostream& data) {
    data.write((char*) &this->lastScanlineCycle, sizeof(this->lastScanlineCycle));
    data.write((char*) &this->lastPhaseCycle, sizeof(this->lastPhaseCycle));
    data.write((char*) &this->lastOamDmaBeginCycle, sizeof(this->lastOamDmaBeginCycle));
    data.write((char*) &this->halfSpeed, sizeof(this->halfSpeed));
    data.write((char*) &this->scanlineX, sizeof(this->scanlineX));
    data.write((char*) &this->oamDmaSrc, sizeof(this->oamDmaSrc));
    data.write((char*) this->expandedBgp, sizeof(this->expandedBgp));
    data.write((char*) this->expandedObp, sizeof(this->expandedObp));
    data.write((char*) this->vram, sizeof(this->vram));
    data.write((char*) this->oam, sizeof(this->oam));
    data.write((char*) this->bgPaletteData, sizeof(this->bgPaletteData));
    data.write((char*) this->sprPaletteData, sizeof(this->sprPaletteData));
}

void PPU::mapBanks() {
    u8 bank = (u8) (this->gameboy->gbMode == MODE_CGB && (this->gameboy->mmu->readIO(VBK) & 0x1) != 0);
    this->gameboy->mmu->mapBankBlock(0x8, this->vram[bank] + 0x0000);
    this->gameboy->mmu->mapBankBlock(0x9, this->vram[bank] + 0x1000);
}

inline void PPU::checkLYC() {
    u8 stat = this->gameboy->mmu->readIO(STAT);
    if(this->gameboy->mmu->readIO(LY) == this->gameboy->mmu->readIO(LYC)) {
        this->gameboy->mmu->writeIO(STAT, (u8) (stat | 4));
        if(stat & 0x40) {
            this->gameboy->mmu->writeIO(IF, (u8) (this->gameboy->mmu->readIO(IF) | INT_LCD));
        }
    } else {
        this->gameboy->mmu->writeIO(STAT, (u8) (stat & ~4));
    }
}

void PPU::update() {
    if(this->lastOamDmaBeginCycle != 0) {
        while((this->oamDmaSrc & 0xFF) < 0xA0 && this->gameboy->cpu->getCycle() >= this->lastOamDmaBeginCycle + (this->oamDmaSrc & 0xFF) + 1) {
            this->oam[this->oamDmaSrc & 0xFF] = this->gameboy->mmu->read(this->oamDmaSrc);
            this->oamDmaSrc++;
        }

        if((this->oamDmaSrc & 0xFF) < 0xA0) {
            this->gameboy->cpu->setEventCycle(this->lastOamDmaBeginCycle + 0xA0);
        } else {
            this->lastOamDmaBeginCycle = 0;
        }
    }

    if((this->gameboy->mmu->readIO(LCDC) & 0x80) == 0) {
        this->lastScanlineCycle = this->gameboy->cpu->getCycle();

        this->gameboy->mmu->writeIO(LY, 0);
        this->gameboy->mmu->writeIO(STAT, (u8) (this->gameboy->mmu->readIO(STAT) & 0xF8));

        // Ensure that we continue to return execution to frontend every frame, despite the LCD being off.
        while(this->gameboy->cpu->getCycle() >= this->lastPhaseCycle + (CYCLES_PER_FRAME << this->halfSpeed)) {
            this->lastPhaseCycle += CYCLES_PER_FRAME << this->halfSpeed;

            this->gameboy->ranFrame = true;
        }

        if(this->gameboy->ranFrame && this->gameboy->settings.frameBuffer != NULL) {
            for(u8 y = 40; y < 184; y++) {
                memset(&this->gameboy->settings.frameBuffer[y * this->gameboy->settings.framePitch + 48], 0xFF, 160 * sizeof(u32));
            }
        }

        this->gameboy->cpu->setEventCycle(this->lastPhaseCycle + (CYCLES_PER_FRAME << this->halfSpeed));
    } else {
        this->lastPhaseCycle = this->gameboy->cpu->getCycle();

        while(this->gameboy->cpu->getCycle() >= this->lastScanlineCycle + (modeCycles[this->gameboy->mmu->readIO(STAT) & 3] << this->halfSpeed)) {
            this->updateScanline();

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

                        u8 interrupts = this->gameboy->mmu->readIO(IF);
                        interrupts |= INT_VBLANK;
                        if(stat & 0x30) {
                            interrupts |= INT_LCD;
                        }

                        this->gameboy->mmu->writeIO(IF, interrupts);
                        this->gameboy->ranFrame = true;
                    } else {
                        this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_ACCESS_OAM));

                        if(stat & 0x20) {
                            this->gameboy->mmu->writeIO(IF, (u8) (this->gameboy->mmu->readIO(IF) | INT_LCD));
                        }
                    }

                    break;
                case LCD_VBLANK:
                    if(ly == 0) {
                        this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_ACCESS_OAM));

                        if(stat & 0x20) {
                            this->gameboy->mmu->writeIO(IF, (u8) (this->gameboy->mmu->readIO(IF) | INT_LCD));
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
                    this->scanlineX = 0;
                    this->updateLineSprites();

                    this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_ACCESS_OAM_VRAM));
                    break;
                case LCD_ACCESS_OAM_VRAM:
                    if(!this->gameboy->settings.perPixelRendering && this->gameboy->settings.drawEnabled) {
                        this->drawScanline(ly);
                    }

                    this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_HBLANK));

                    if(stat & 0x8) {
                        this->gameboy->mmu->writeIO(IF, (u8) (this->gameboy->mmu->readIO(IF) | INT_LCD));
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

        this->updateScanline();
    }
}

void PPU::setHalfSpeed(bool halfSpeed) {
    if(!this->halfSpeed && halfSpeed) {
        this->lastScanlineCycle -= this->gameboy->cpu->getCycle() - this->lastScanlineCycle;
        this->lastPhaseCycle -= this->gameboy->cpu->getCycle() - this->lastPhaseCycle;
    } else if(this->halfSpeed && !halfSpeed) {
        this->lastScanlineCycle += (this->gameboy->cpu->getCycle() - this->lastScanlineCycle) >> 1;
        this->lastPhaseCycle += (this->gameboy->cpu->getCycle() - this->lastPhaseCycle) >> 1;
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

inline void PPU::updateLineTile(u8 map, u8 x, u8 y) {
    TileLine* line = &this->currTileLines[map];

    u8 lcdc = this->gameboy->mmu->readIO(LCDC);

    u8 tileX = x >> 3;
    u8 tileY = y >> 3;
    u16 offset = (u16) (0x1800 + (map << 10) + (((tileY << 5) + tileX) & 0x3FF));

    u8 tileVal = this->vram[0][offset];
    u16 tile = (lcdc & 0x10) != 0 ? tileVal : (u16) ((s8) tileVal + 0x100);

    u8 bank = 0;
    bool flipX = false;
    u8 ty = (u8) (y & 7);
    u8 baseDepth = 1;

    if(this->gameboy->gbMode == MODE_CGB) {
        u8 flags = this->vram[1][offset];

        line->palette = (u8) (flags & 7);
        bank = (u8) ((flags >> 3) & 1);
        flipX = (bool) ((flags >> 5) & 1);
        ty ^= (u8) (((flags >> 6) & 1) * 7);
        baseDepth = (u8) ((((flags >> 6) & 2) + 1) * (lcdc & 0x01));
    }

    u16 pxOffset = (u16) ((tile << 4) + (ty << 1));

    u8 b1 = this->vram[bank][pxOffset];
    u8 b2 = this->vram[bank][pxOffset + 1];

    if(!flipX) {
        b1 = BitReverseTable256[b1];
        b2 = BitReverseTable256[b2];
    }

    u16 pxData = (u16) (BitStretchTable256[b1] | (BitStretchTable256[b2] << 1));

    for(u8 tx = 0; tx < 8; tx++) {
        u8 color = (u8) ((pxData >> (tx << 1)) & 3);
        line->color[tx] = color;
        line->depth[tx] = (u8) ((baseDepth - depthOffset[color]) & 3);
    }
}

inline void PPU::updateLineSprites() {
    u8 ly = this->gameboy->mmu->readIO(LY);

    bool large = (this->gameboy->mmu->readIO(LCDC) & 4) != 0;
    u8 height = (u8) (large ? 16 : 8);

    this->currSprites = 0;
    for(u8 offset = 0; offset < 0xA0 && this->currSprites < 10; offset += 4) {
        SpriteLine* line = &this->currSpriteLines[this->currSprites];

        u8 y = (u8) (this->oam[offset + 0] - 16);
        u8 ty = (u8) (ly - y);
        if(ty >= height) {
            continue;
        }

        line->x = (u8) (this->oam[offset + 1] - 8);
        u8 tile = (u8) (this->oam[offset + 2] & ~large);
        u8 flags = this->oam[offset + 3];

        line->palette = (u8) (flags & 7);
        u8 bank = (u8) ((flags >> 3) & 1);
        line->obp = (u8) ((flags >> 4) & 1);
        bool flipX = (bool) ((flags >> 5) & 1);
        ty ^= (u8) (((flags >> 6) & 1) * (height - 1));
        u8 depth = (u8) (2 - ((flags >> 6) & 2));

        u16 pxOffset = (u16) ((tile << 4) + (ty << 1));

        u8 b1 = this->vram[bank][pxOffset];
        u8 b2 = this->vram[bank][pxOffset + 1];

        if(!flipX) {
            b1 = BitReverseTable256[b1];
            b2 = BitReverseTable256[b2];
        }

        u16 pxData = (u16) (BitStretchTable256[b1] | (BitStretchTable256[b2] << 1));

        for(u8 tx = 0; tx < 8; tx++) {
            line->color[tx] = (u8) ((pxData >> (tx << 1)) & 3);
            line->depth[tx] = depth;
        }

        this->currSprites++;
    }
}

inline void PPU::updateScanline() {
    bool drawing = this->gameboy->settings.perPixelRendering && this->gameboy->settings.drawEnabled && (this->gameboy->mmu->readIO(STAT) & 3) == LCD_ACCESS_OAM_VRAM;
    if(drawing && this->scanlineX < 160) {
        while(this->scanlineX < 160 && this->gameboy->cpu->getCycle() >= this->lastScanlineCycle + ((this->scanlineX + 7) << this->halfSpeed)) {
            this->drawPixel(this->scanlineX, this->gameboy->mmu->readIO(LY));
            this->scanlineX++;
        }
    }

    if(drawing && this->scanlineX < 160) {
        this->gameboy->cpu->setEventCycle(this->lastScanlineCycle + ((this->scanlineX + 7) << this->halfSpeed));
    } else {
        this->gameboy->cpu->setEventCycle(this->lastScanlineCycle + (modeCycles[this->gameboy->mmu->readIO(STAT) & 3] << this->halfSpeed));
    }
}

inline void PPU::drawPixel(u8 x, u8 y) {
    if(this->gameboy->settings.frameBuffer == NULL) {
        return;
    }

    u32* colorOut = &this->gameboy->settings.frameBuffer[(y + 40) * this->gameboy->settings.framePitch + (x + 48)];

    switch(this->gameboy->sgb->getGfxMask()) {
        case 0: {
            u8 lcdc = this->gameboy->mmu->readIO(LCDC);
            if((lcdc & 0x80) != 0) {
                u16 colorDst = 0;
                u8 depthDst = 0;

                u16* baseBgPalette = this->gameboy->gbMode != MODE_GB || !this->gameboy->mmu->isBiosMapped() ? (u16*) this->bgPaletteData : grayScalePalette;
                u16* baseSprPalette = this->gameboy->gbMode != MODE_GB || !this->gameboy->mmu->isBiosMapped() ? (u16*) this->sprPaletteData : grayScalePalette;

                // Background
                if(this->gameboy->gbMode == MODE_CGB || (lcdc & 0x01) != 0) {
                    u8 map = (u8) ((lcdc >> 3) & 1);
                    u8 pixelX = (u8) ((x + this->gameboy->mmu->readIO(SCX)) & 0xFF);
                    u8 pixelY = (u8) ((y + this->gameboy->mmu->readIO(SCY)) & 0xFF);
                    u8 subX = (u8) (pixelX & 7);

                    TileLine* line = &this->currTileLines[map];
                    if(subX == 0 || x == 0) {
                        this->updateLineTile(map, pixelX, pixelY);
                    }

                    u8 palette = this->gameboy->gbMode == MODE_SGB ? this->gameboy->sgb->getPaletteMap()[(y >> 3) * 20 + (x >> 3)] : line->palette;

                    colorDst = baseBgPalette[(palette << 2) + this->expandedBgp[line->color[subX]]];
                    depthDst = line->depth[subX];
                }

                // Window
                if((lcdc & 0x20) != 0) {
                    u8 wx = this->gameboy->mmu->readIO(WX);
                    u8 wy = this->gameboy->mmu->readIO(WY);
                    if(y >= wy && x >= wx - 7) {
                        u8 map = (u8) ((lcdc >> 6) & 1);
                        u8 pixelX = (u8) ((x - wx + 7) & 0xFF);
                        u8 pixelY = (u8) ((y - wy) & 0xFF);
                        u8 subX = (u8) (pixelX & 7);

                        TileLine* line = &this->currTileLines[map];
                        if(subX == 0 || x == 0) {
                            this->updateLineTile(map, pixelX, pixelY);
                        }

                        u8 palette = this->gameboy->gbMode == MODE_SGB ? this->gameboy->sgb->getPaletteMap()[(y >> 3) * 20 + (x >> 3)] : line->palette;

                        colorDst = baseBgPalette[(palette << 2) + this->expandedBgp[line->color[subX]]];
                        depthDst = line->depth[subX];
                    }
                }

                // Sprites
                if((lcdc & 0x02) != 0) {
                    for(s8 spriteId = (s8) (this->currSprites - 1); spriteId >= 0; spriteId--) {
                        SpriteLine* line = &this->currSpriteLines[spriteId];

                        u8 subX = x - line->x;
                        if(subX > 7) {
                            continue;
                        }

                        u8 palette = line->palette;
                        if(this->gameboy->gbMode == MODE_SGB) {
                            palette += this->gameboy->sgb->getPaletteMap()[(y >> 3) * 20 + (x >> 3)];
                        }

                        u8 color = line->color[subX];
                        u8 depth = line->depth[subX];

                        if(color != 0 && depth >= depthDst) {
                            colorDst = baseSprPalette[(palette << 2) + this->expandedObp[(line->obp << 2) + color]];
                            depthDst = depth;
                        }
                    }
                }

                u32 outputColor = RGB555ToRGBA8888Table[colorDst];
                if(this->gameboy->settings.emulateBlur) {
                    u32 oldColor = *colorOut;
                    *colorOut = (u32) (((u64) outputColor + (u64) oldColor - ((outputColor ^ oldColor) & 0x01010101)) >> 1);
                } else {
                    *colorOut = outputColor;
                }
            }

            break;
        }
        case 2:
            *colorOut = 0;
            break;
        case 3:
            *colorOut = RGB555ToRGBA8888Table[this->bgPaletteData[this->gameboy->mmu->readIO(BGP) & 3]];
            break;
        default:
            break;
    }
}

inline void PPU::drawScanline(u8 scanline) {
    if(this->gameboy->settings.frameBuffer == NULL) {
        return;
    }

    u32* lineBuffer = &this->gameboy->settings.frameBuffer[(scanline + 40) * this->gameboy->settings.framePitch];

    switch(this->gameboy->sgb->getGfxMask()) {
        case 0: {
            u8 lcdc = this->gameboy->mmu->readIO(LCDC);
            if((lcdc & 0x80) != 0) {
                u8 depthBuffer[160];

                u16* baseBgPalette = this->gameboy->gbMode != MODE_GB || !this->gameboy->mmu->isBiosMapped() ? (u16*) this->bgPaletteData : grayScalePalette;
                u16* baseSprPalette = this->gameboy->gbMode != MODE_GB || !this->gameboy->mmu->isBiosMapped() ? (u16*) this->sprPaletteData : grayScalePalette;

                // Background
                if(this->gameboy->gbMode == MODE_CGB || (lcdc & 0x01) != 0) {
                    u8* subSgbMap = &this->gameboy->sgb->getPaletteMap()[(scanline >> 3) * 20];

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
                            depthBuffer[pixelX] = depth - (u8) (colorId == 0);

                            u32 outputColor = RGB555ToRGBA8888Table[baseBgPalette[(paletteIds[(pixelX >> 3) - tileX + 1] << 2) + this->expandedBgp[colorId]]];
                            u32* colorOut = &lineBuffer[pixelX];
                            if(this->gameboy->settings.emulateBlur) {
                                u32 oldColor = *colorOut;
                                *colorOut = (u32) (((u64) outputColor + (u64) oldColor - ((outputColor ^ oldColor) & 0x01010101)) >> 1);
                            } else {
                                *colorOut = outputColor;
                            }
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
                                depthBuffer[pixelX] = depth - (u8) (colorId == 0);

                                u32 outputColor = RGB555ToRGBA8888Table[baseBgPalette[(paletteIds[(pixelX >> 3) - tileX + 1] << 2) + this->expandedBgp[colorId]]];
                                u32* colorOut = &lineBuffer[pixelX];
                                if(this->gameboy->settings.emulateBlur) {
                                    u32 oldColor = *colorOut;
                                    *colorOut = (u32) (((u64) outputColor + (u64) oldColor - ((outputColor ^ oldColor) & 0x01010101)) >> 1);
                                } else {
                                    *colorOut = outputColor;
                                }
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

                    u8 spriteHeight = (u8) ((lcdc & 0x04) != 0 ? 16 : 8);

                    for(s8 sprite = 39; sprite >= 0; sprite--) {
                        u8 spriteY = this->oam[sprite * 4 + 0];

                        if(scanline + 16 < spriteY || scanline + 16 >= spriteY + spriteHeight) {
                            continue;
                        }

                        u8 spriteX = this->oam[sprite * 4 + 1];
                        u8 tileId = this->oam[sprite * 4 + 2];
                        u8 flags = this->oam[sprite * 4 + 3];

                        s16 basePixelX = (s16) (spriteX - 8);
                        s8 baseTileX = (s8) (basePixelX >> 3);

                        u8 pixelY = (u8) (scanline - spriteY + 16);
                        u8 tileY = pixelY >> 3;
                        u8 subTileY = (u8) (pixelY & 7);

                        u8 paletteId = (u8) (flags & 7);
                        u8 bank = (u8) ((flags >> 3) & 1);
                        u8 obpId = (u8) ((flags >> 4) & 1);
                        bool flipX = (bool) ((flags >> 5) & 1);
                        bool flipY = (bool) ((flags >> 6) & 1);
                        u8 depth = (u8) (2 - ((flags >> 6) & 2));

                        if(spriteHeight == 16) {
                            tileId = (u8) (((tileId & ~1) + tileY) ^ flipY);
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
                                depthBuffer[pixelX] = depth;

                                u32 outputColor = RGB555ToRGBA8888Table[baseSprPalette[(paletteIds[(pixelX >> 3) - baseTileX] << 2) + this->expandedObp[(obpId << 2) + colorId]]];
                                u32* colorOut = &lineBuffer[pixelX];
                                if(this->gameboy->settings.emulateBlur) {
                                    u32 oldColor = *colorOut;
                                    *colorOut = (u32) (((u64) outputColor + (u64) oldColor - ((outputColor ^ oldColor) & 0x01010101)) >> 1);
                                } else {
                                    *colorOut = outputColor;
                                }
                            }
                        }
                    }
                }
            }

            break;
        }
        case 2:
            memset(lineBuffer, 0, this->gameboy->settings.framePitch * sizeof(u32));
            break;
        case 3: {
            u32 clearColor = RGB555ToRGBA8888Table[this->bgPaletteData[this->gameboy->mmu->readIO(BGP) & 3]];
            for(u32 i = 0; i < this->gameboy->settings.framePitch; i++) {
                lineBuffer[i] = clearColor;
            }

            break;
        }
        default:
            break;
    }
}