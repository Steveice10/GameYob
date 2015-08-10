/* Known graphical issues(?):
 * DMG sprite order
 * Horizontal window split behavior
 */

#include <string.h>

#include "platform/gfx.h"
#include "gameboy.h"
#include "ppu.h"
#include "romfile.h"

#define FLIP_Y (0x40)
#define FLIP_X (0x20)
#define PRIORITY (0x80)

static const u32 depthOffset[4] =
        {
                0x01, 0x00, 0x00, 0x00
        };

static const u8 BitReverseTable256[] =
        {
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

static const u32 BitStretchTable256[] =
        {
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

GameboyPPU::GameboyPPU(Gameboy* gb) {
    this->gameboy = gb;
}

void GameboyPPU::initPPU() {
    bgPalettes[0][0] = RGBA32(255, 255, 255);
    bgPalettes[0][1] = RGBA32(192, 192, 192);
    bgPalettes[0][2] = RGBA32(94, 94, 94);
    bgPalettes[0][3] = RGBA32(0, 0, 0);
    sprPalettes[0][0] = RGBA32(255, 255, 255);
    sprPalettes[0][1] = RGBA32(192, 192, 192);
    sprPalettes[0][2] = RGBA32(94, 94, 94);
    sprPalettes[0][3] = RGBA32(0, 0, 0);
    sprPalettes[1][0] = RGBA32(255, 255, 255);
    sprPalettes[1][1] = RGBA32(192, 192, 192);
    sprPalettes[1][2] = RGBA32(94, 94, 94);
    sprPalettes[1][3] = RGBA32(0, 0, 0);

    gfxMask = 0;

    memset(bgPalettesModified, 0, sizeof(bgPalettesModified));
    memset(sprPalettesModified, 0, sizeof(sprPalettesModified));
}

void GameboyPPU::refreshPPU() {
    for(int i = 0; i < 8; i++) {
        bgPalettesModified[i] = true;
        sprPalettesModified[i] = true;
    }
}

void GameboyPPU::clearPPU() {
    gfxClearScreenBuffer(0x00, 0x00, 0x00);
}

void GameboyPPU::drawScanline(int scanline) {
    if((gfxGetFastForward() && fastForwardCounter < fastForwardFrameSkip) || gameboy->getRomFile()->isGBS()) {
        return;
    }

    for(int i = 0; i < 8; i++) {
        if(bgPalettesModified[i]) {
            if(gameboy->gbMode == GB) {
                updateBgPaletteDMG();
            } else {
                updateBgPalette(i);
            }

            bgPalettesModified[i] = false;
        }

        if(sprPalettesModified[i]) {
            if(gameboy->gbMode == GB) {
                updateSprPaletteDMG(i);
            } else {
                updateSprPalette(i);
            }

            sprPalettesModified[i] = false;
        }
    }

    lineBuffer = gfxGetLineBuffer(scanline);

    if(gfxMask == 0) {
        memset(depthBuffer, 0, 160 * sizeof(u32));
        subSgbMap = &gameboy->sgbMap[scanline / 8 * 20];

        int winX = gameboy->ioRam[0x4B] - 7;
        int winY = gameboy->ioRam[0x4A];
        bool drawingWindow = winY <= scanline && winY < 144 && winX >= 0 && winX < 160 && gameboy->ioRam[0x40] & 0x20;
        bool tileSigned = !(gameboy->ioRam[0x40] & 0x10); // Tile Data location

        drawBackground(scanline, winX, winY, drawingWindow, tileSigned);
        drawWindow(scanline, winX, winY, drawingWindow, tileSigned);
        drawSprites(scanline);
    } else if(gfxMask == 2) {
        memset(lineBuffer, 0x00, 160 * sizeof(u32));
        return;
    } else if(gfxMask == 3) {
        wmemset((wchar_t*) lineBuffer, (wchar_t) bgPalettes[0][0], 160);
        return;
    }
}

void GameboyPPU::drawBackground(int scanline, int winX, int winY, bool drawingWindow, bool tileSigned) {
    if(gameboy->gbMode == CGB || (gameboy->ioRam[0x40] & 1) != 0) { // Background enabled
        u8 scrollX = gameboy->ioRam[0x43];
        int scrollY = gameboy->ioRam[0x42];

        // The y position (measured in tiles)
        int tileY = ((scanline + scrollY) & 0xFF) / 8;

        // Tile Map address plus row offset
        int BGMapAddr;
        if(gameboy->ioRam[0x40] & 0x8) {
            BGMapAddr = 0x1C00 + (tileY * 32);
        } else {
            BGMapAddr = 0x1800 + (tileY * 32);
        }

        // Number of tiles to draw in a row
        int numTilesX = 20;
        if(drawingWindow) {
            numTilesX = (winX + 7) / 8;
        }

        // Tiles to draw
        int startTile = scrollX / 8;
        int endTile = (startTile + numTilesX + 1) & 31;

        // Calculate lineBuffer Start, negatives treated as unsigned for speed up
        u32 writeX = (u32) (-(scrollX & 0x07));
        for(int i = startTile; i != endTile; i = (i + 1) & 31) {
            // The address (from beginning of gameboy->vram) of the tile's mapping
            int mapAddr = BGMapAddr + i;

            // This is the tile id.
            int tileNum = gameboy->vram[0][mapAddr];
            if(tileSigned) {
                tileNum = ((s8) tileNum) + 256;
            }

            // Setup Tile Info
            u32 flag = 0;
            u32 bank = 0;
            u32 paletteid = 0;
            if(gameboy->gbMode == CGB) {
                flag = gameboy->vram[1][mapAddr];
                bank = (u32) ((flag & 0x8) != 0);
                paletteid = flag & 0x7;
            }

            // This is the tile's Y position to be read (0-7)
            int pixelY = (scanline + scrollY) & 0x07;
            if(flag & FLIP_Y) {
                pixelY = 7 - pixelY;
            }

            // Read bytes of tile line
            u32 vRamB1 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1)];
            u32 vRamB2 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1) + 1];

            // Reverse their bits if flipX set
            if(!(flag & FLIP_X)) {
                vRamB1 = BitReverseTable256[vRamB1];
                vRamB2 = BitReverseTable256[vRamB2];
            }

            // Mux the bits to for more logical pixels
            u32 pxData = BitStretchTable256[vRamB1] | (BitStretchTable256[vRamB2] << 1);

            // Setup depth based on priority
            u32 depth = 1;
            if(flag & PRIORITY) {
                depth = 3;
            }

            // Split Render mode based on sgbMode, slight speed up
            if(gameboy->sgbMode) {
                for(int x = 0; x < 16; x += 2, writeX++) {
                    if(writeX >= 160) {
                        continue;
                    }

                    u32 colorid = (pxData >> x) & 0x03;

                    // Draw pixel
                    depthBuffer[writeX] = depth - depthOffset[colorid];
                    lineBuffer[writeX] = bgPalettes[subSgbMap[writeX >> 3]][colorid];
                }
            } else {
                for(int x = 0; x < 16; x += 2, writeX++) {
                    if(writeX >= 160) {
                        continue;
                    }

                    u32 colorid = (pxData >> x) & 0x03;

                    // Draw pixel
                    depthBuffer[writeX] = depth - depthOffset[colorid];
                    lineBuffer[writeX] = bgPalettes[paletteid][colorid];
                }
            }
        }
    }
}

void GameboyPPU::drawWindow(int scanline, int winX, int winY, bool drawingWindow, bool tileSigned) {
    if(drawingWindow) { // Window enabled
        // The y position (measured in tiles)
        int tileY = (scanline - winY) / 8;

        // Tile Map address plus row offset
        int winMapAddr;
        if(gameboy->ioRam[0x40] & 0x40) {
            winMapAddr = 0x1C00 + (tileY * 32);
        } else {
            winMapAddr = 0x1800 + (tileY * 32);
        }

        // Tiles to draw
        int endTile = 21 - winX / 8;

        // Calculate lineBuffer Start, negatives treated as unsigned for speed up
        u32 writeX = (u32) winX;
        for(int i = 0; i < endTile; i++) {
            // The address (from beginning of gameboy->vram) of the tile's mapping
            int mapAddr = winMapAddr + i;

            // This is the tile id.
            int tileNum = gameboy->vram[0][mapAddr];
            if(tileSigned) {
                tileNum = ((s8) tileNum) + 128 + 0x80;
            }

            // Setup Tile Info
            u32 flag = 0;
            u32 bank = 0;
            u32 paletteid = 0;
            if(gameboy->gbMode == CGB) {
                flag = gameboy->vram[1][mapAddr];
                bank = (u32) ((flag & 0x8) != 0);
                paletteid = flag & 0x7;
            }

            // This is the tile's Y position to be read (0-7)
            int pixelY = (scanline - winY) & 0x07;
            if(flag & FLIP_Y) {
                pixelY = 7 - pixelY;
            }

            // Read bytes of tile line
            u32 vRamB1 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1)];
            u32 vRamB2 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1) + 1];

            // Reverse their bits if flipX set
            if(!(flag & FLIP_X)) {
                vRamB1 = BitReverseTable256[vRamB1];
                vRamB2 = BitReverseTable256[vRamB2];
            }

            // Mux the bits to for more logical pixels
            u32 pxData = BitStretchTable256[vRamB1] | (BitStretchTable256[vRamB2] << 1);

            // Setup depth based on priority
            u32 depth = 1;
            if(flag & PRIORITY) {
                depth = 3;
            }

            // Split Render mode based on sgbMode, slight speed up
            if(gameboy->sgbMode) {
                for(int x = 0; x < 16; x += 2, writeX++) {
                    if(writeX >= 160) {
                        continue;
                    }

                    u32 colorid = (pxData >> x) & 0x03;

                    // Draw pixel
                    depthBuffer[writeX] = depth - depthOffset[colorid];
                    lineBuffer[writeX] = bgPalettes[subSgbMap[writeX >> 3]][colorid];
                }
            } else {
                for(int x = 0; x < 16; x += 2, writeX++) {
                    if(writeX >= 160) {
                        continue;
                    }

                    u32 colorid = (pxData >> x) & 0x03;

                    // Draw pixel
                    depthBuffer[writeX] = depth - depthOffset[colorid];
                    lineBuffer[writeX] = bgPalettes[paletteid][colorid];
                }
            }
        }
    }
}

void GameboyPPU::drawSprites(int scanline) {
    if(gameboy->ioRam[0x40] & 0x2) { // Sprites enabled
        for(int i = 39; i >= 0; i--) {
            // The sprite's number, times 4 (each uses 4 bytes)
            int spriteOffset = i * 4;

            int y = (gameboy->hram[spriteOffset] - 16);
            int height = (gameboy->ioRam[0x40] & 0x4) ? 16 : 8;

            // Clip Sprite to or bottom
            if(scanline < y || scanline >= y + height) {
                continue;
            }

            // Setup Tile Info
            int tileNum = gameboy->hram[spriteOffset + 2];
            u32 flag = gameboy->hram[spriteOffset + 3];
            int bank = 0;
            int paletteid;
            if(gameboy->gbMode == CGB) {
                bank = (gameboy->hram[spriteOffset + 3] & 0x8) >> 3;
                paletteid = gameboy->hram[spriteOffset + 3] & 0x7;
            } else {
                paletteid = (gameboy->hram[spriteOffset + 3] & 0x10) >> 2;
            }

            // Select Tile base on Tiles Y offset
            if(height == 16) {
                tileNum &= ~1;
                if(scanline - y >= 8) {
                    tileNum++;
                }
            }

            // This is the tile's Y position to be read (0-7)
            int pixelY = (scanline - y) & 0x07;
            if(flag & FLIP_Y) {
                pixelY = 7 - pixelY;
                if(height == 16) {
                    tileNum = tileNum ^ 1;
                }
            }

            // Read bytes of tile line
            u32 vRamB1 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1)];
            u32 vRamB2 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1) + 1];

            // Reverse their bits if flipX set
            if(!(flag & FLIP_X)) {
                vRamB1 = BitReverseTable256[vRamB1];
                vRamB2 = BitReverseTable256[vRamB2];
            }

            // Mux the bits to for more logical pixels
            u32 pxData = BitStretchTable256[vRamB1] | (BitStretchTable256[vRamB2] << 1);

            // Setup depth based on priority
            u32 depth = 2;
            if(flag & PRIORITY) {
                depth = 0;
            }

            // Calculate where to start to draw, negatives treated as unsigned for speed up
            u32 writeX = (u32) ((s32) (gameboy->hram[spriteOffset + 1] - 8));

            // Split Render mode based on sgbMode, slight speed up
            if(gameboy->sgbMode) {
                for(int j = 0; j < 16; j += 2, writeX++) {
                    if(writeX >= 160) {
                        continue;
                    }

                    u32 colorid = (pxData >> j) & 0x03;

                    // Draw pixel, If not transparent or above depth buffer
                    if(colorid != 0 && depth >= depthBuffer[writeX]) {
                        depthBuffer[writeX] = depth;
                        lineBuffer[writeX] = sprPalettes[paletteid + subSgbMap[writeX >> 3]][colorid];
                    }
                }
            } else {
                for(int j = 0; j < 16; j += 2, writeX++) {
                    if(writeX >= 160) {
                        continue;
                    }

                    u32 colorid = (pxData >> j) & 0x03;

                    // Draw pixel, If not transparent or above depth buffer
                    if(colorid != 0 && depth >= depthBuffer[writeX]) {
                        depthBuffer[writeX] = depth;
                        lineBuffer[writeX] = sprPalettes[paletteid][colorid];
                    }
                }
            }
        }
    }
}

void GameboyPPU::drawScreen() {
    if(gfxGetFastForward() && fastForwardCounter++ < fastForwardFrameSkip) {
        return;
    } else {
        fastForwardCounter = 0;
    }

    if(!gameboy->getRomFile()->isGBS()) {
        gfxDrawScreen();
    } else if(!gfxGetFastForward()) {
        gfxWaitForVBlank();
    }
}

void GameboyPPU::setSgbMask(int mask) {
    gfxMask = (u8) mask;
}

void GameboyPPU::setSgbTiles(u8* src, u8 flags) {
}

void GameboyPPU::setSgbMap(u8* src) {
}

void GameboyPPU::writeVram(u16 addr, u8 val) {
}

void GameboyPPU::writeVram16(u16 addr, u16 src) {
}

void GameboyPPU::writeHram(u16 addr, u8 val) {
}

void GameboyPPU::handleVideoRegister(u8 ioReg, u8 val) {
    switch(ioReg) {
        case 0x40:
            if((gameboy->ioRam[ioReg] & 0x80) && !(val & 0x80)) {
                if(gameboy->gbMode == GB) {
                    int red = (gameboy->bgPaletteData[0] & 0x1F) * 8;
                    int green = (((gameboy->bgPaletteData[0] & 0xE0) >> 5) | ((gameboy->bgPaletteData[1]) & 0x3) << 3) * 8;
                    int blue = ((gameboy->bgPaletteData[1] >> 2) & 0x1F) * 8;
                    gfxClearScreenBuffer((u8) red, (u8) green, (u8) blue);
                } else {
                    gfxClearScreenBuffer(0xFF, 0xFF, 0xFF);
                }
            }

            break;
        case 0x47:
            if(gameboy->gbMode == GB) {
                bgPalettesModified[0] = true;
            }

            break;
        case 0x48:
            if(gameboy->gbMode == GB) {
                sprPalettesModified[0] = true;
                if(gameboy->sgbMode) {
                    sprPalettesModified[1] = true;
                    sprPalettesModified[2] = true;
                    sprPalettesModified[3] = true;
                }
            }

            break;
        case 0x49:
            if(gameboy->gbMode == GB) {
                sprPalettesModified[4] = true;
                if(gameboy->sgbMode) {
                    sprPalettesModified[5] = true;
                    sprPalettesModified[6] = true;
                    sprPalettesModified[7] = true;
                }
            }

            break;
        case 0x69:
            if(gameboy->gbMode == CGB) {
                bgPalettesModified[(gameboy->ioRam[0x68] / 8) & 7] = true;
            }

            break;
        case 0x6B:
            if(gameboy->gbMode == CGB) {
                sprPalettesModified[(gameboy->ioRam[0x6A] / 8) & 7] = true;
            }

            break;
        default:
            break;
    }
}

void GameboyPPU::updateBgPalette(int paletteid) {
    int multiplier = 8;
    for(int i = 0; i < 4; i++) {
        int red = (gameboy->bgPaletteData[(paletteid * 8) + (i * 2)] & 0x1F) * multiplier;
        int green = (((gameboy->bgPaletteData[(paletteid * 8) + (i * 2)] & 0xE0) >> 5) | ((gameboy->bgPaletteData[(paletteid * 8) + (i * 2) + 1]) & 0x3) << 3) * multiplier;
        int blue = ((gameboy->bgPaletteData[(paletteid * 8) + (i * 2) + 1] >> 2) & 0x1F) * multiplier;
        bgPalettes[paletteid][i] = RGBA32(red, green, blue);
    }
}

void GameboyPPU::updateBgPaletteDMG() {
    u8 val = gameboy->ioRam[0x47];
    u8 palette[] = {(u8) (val & 3), (u8) ((val >> 2) & 3), (u8) ((val >> 4) & 3), (u8) (val >> 6)};

    int multiplier = 8;
    int howmany = gameboy->sgbMode ? 4 : 1;
    for(int j = 0; j < howmany; j++) {
        for(int i = 0; i < 4; i++) {
            u8 col = palette[i];
            int red = (gameboy->bgPaletteData[(j * 8) + (col * 2)] & 0x1F) * multiplier;
            int green = (((gameboy->bgPaletteData[(j * 8) + (col * 2)] & 0xE0) >> 5) | ((gameboy->bgPaletteData[(j * 8) + (col * 2) + 1]) & 0x3) << 3) * multiplier;
            int blue = ((gameboy->bgPaletteData[(j * 8) + (col * 2) + 1] >> 2) & 0x1F) * multiplier;
            bgPalettes[j][i] = RGBA32(red, green, blue);
        }
    }
}

void GameboyPPU::updateSprPalette(int paletteid) {
    int multiplier = 8;
    for(int i = 0; i < 4; i++) {
        int red = (gameboy->sprPaletteData[(paletteid * 8) + (i * 2)] & 0x1F) * multiplier;
        int green = (((gameboy->sprPaletteData[(paletteid * 8) + (i * 2)] & 0xE0) >> 5) | ((gameboy->sprPaletteData[(paletteid * 8) + (i * 2) + 1]) & 0x3) << 3) * multiplier;
        int blue = ((gameboy->sprPaletteData[(paletteid * 8) + (i * 2) + 1] >> 2) & 0x1F) * multiplier;
        sprPalettes[paletteid][i] = RGBA32(red, green, blue);
    }
}

void GameboyPPU::updateSprPaletteDMG(int paletteid) {
    u8 val = gameboy->ioRam[0x48 + paletteid / 4];
    u8 palette[] = {(u8) (val & 3), (u8) ((val >> 2) & 3), (u8) ((val >> 4) & 3), (u8) (val >> 6)};

    int multiplier = 8;
    for(int i = 0; i < 4; i++) {
        u8 col = palette[i];
        int red = (gameboy->sprPaletteData[(paletteid * 8) + (col * 2)] & 0x1F) * multiplier;
        int green = (((gameboy->sprPaletteData[(paletteid * 8) + (col * 2)] & 0xE0) >> 5) | ((gameboy->sprPaletteData[(paletteid * 8) + (col * 2) + 1]) & 0x3) << 3) * multiplier;
        int blue = ((gameboy->sprPaletteData[(paletteid * 8) + (col * 2) + 1] >> 2) & 0x1F) * multiplier;
        sprPalettes[paletteid][i] = RGBA32(red, green, blue);
    }
}

