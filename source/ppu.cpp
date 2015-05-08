/* Known graphical issues:
 * DMG sprite order
 * Horizontal window split behavior
 * Needs white screen on LCD disable
 */

#include <string.h>

#include "platform/gfx.h"
#include "platform/input.h"
#include "ui/config.h"
#include "gameboy.h"

#define RGBA32(r, g, b) ((u32) ((r) << 24 | (g) << 16 | (b) << 8 | 0xFF))

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

    for(int i = 0; i < 8; i++) {
        sprPalettesRef[i][0] = &sprPalettes[i][0];
        sprPalettesRef[i][1] = &sprPalettes[i][1];
        sprPalettesRef[i][2] = &sprPalettes[i][2];
        sprPalettesRef[i][3] = &sprPalettes[i][3];

        bgPalettesRef[i][0] = &bgPalettes[i][0];
        bgPalettesRef[i][1] = &bgPalettes[i][1];
        bgPalettesRef[i][2] = &bgPalettes[i][2];
        bgPalettesRef[i][3] = &bgPalettes[i][3];
    }

    gfxMask = 0;
    screenWasDisabled = false;

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
}

void GameboyPPU::drawScanline(int scanline) {
}

void GameboyPPU::drawScanline_P2(int scanline) {
    int tileSigned;
    int BGMapAddr;
    int winMapAddr;

    subSgbMap = &gameboy->sgbMap[scanline / 8 * 20];
    lineBuffer = gfxGetScreenBuffer() + scanline * 256;

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

    if(screenWasDisabled) {
        // Leave it white for one extra frame
        memset(lineBuffer, 0xFF, 160 * sizeof(u32));
        return;
    }

    if(gameboy->ioRam[0x40] & 0x10) { // Tile Data location
        tileSigned = 0;
    } else {
        tileSigned = 1;
    }

    if(gameboy->ioRam[0x40] & 0x8) { // Tile Map location
        BGMapAddr = 0x1C00;
    } else {
        BGMapAddr = 0x1800;
    }

    if(gameboy->ioRam[0x40] & 0x40) {
        winMapAddr = 0x1C00;
    } else {
        winMapAddr = 0x1800;
    }

    memset(depthBuffer, 0, 160 * sizeof(u32));

    if(gfxMask == 2) {
        memset(lineBuffer, 0x00, 160 * sizeof(u32));
        return;
    } else if(gfxMask == 3) {
        wmemset((wchar_t*) lineBuffer, (wchar_t) bgPalettes[0][0], 160 * sizeof(u32));
        return;
    }

    if(gfxMask == 0) {
        int winX = gameboy->ioRam[0x4B] - 7;
        int winY = gameboy->ioRam[0x4A];
        bool drawingWindow = scanline >= winY && gameboy->ioRam[0x40] & 0x20;

        int BGOn = 1;
        if(gameboy->gbMode != CGB && (gameboy->ioRam[0x40] & 1) == 0) {
            BGOn = 0;
        }

        if(BGOn) {
            u8 scrollX = gameboy->ioRam[0x43];
            int scrollY = gameboy->ioRam[0x42];

            // The y position (measured in tiles)
            int tileY = ((scanline + scrollY) & 0xFF) / 8;

            int numTilesX = 20;
            if(drawingWindow) {
                numTilesX = (winX + 7) / 8;
            }

            int startTile = scrollX / 8;
            int endTile = (startTile + numTilesX + 1) & 31;

            for(int i = startTile; i != endTile; i = (i + 1) & 31) {
                // The address (from beginning of gameboy->vram) of the tile's mapping
                int mapAddr = BGMapAddr + i + (tileY * 32);

                // This is the tile id.
                int tileNum = gameboy->vram[0][mapAddr];
                if(tileSigned) {
                    tileNum = ((s8) tileNum) + 256;
                }

                int flipX = 0;
                int flipY = 0;
                int bank = 0;
                int paletteid = 0;
                int priority = 0;
                if(gameboy->gbMode == CGB) {
                    flipX = gameboy->vram[1][mapAddr] & 0x20;
                    flipY = gameboy->vram[1][mapAddr] & 0x40;
                    bank = (gameboy->vram[1][mapAddr] & 0x8) != 0;
                    paletteid = gameboy->vram[1][mapAddr] & 0x7;
                    priority = gameboy->vram[1][mapAddr] & 0x80;
                }

                // This is the tile's Y position to be read (0-7)
                int pixelY = (scanline + scrollY) & 0x07;
                if(flipY) {
                    pixelY = 7 - pixelY;
                }

                // Read bytes of tile line
                u32 vRamB1 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1)];
                u32 vRamB2 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1) + 1];

                // Reverse their bits if flipX set
                if(!flipX) {
                    vRamB1 = BitReverseTable256[vRamB1];
                    vRamB2 = BitReverseTable256[vRamB2];
                }

                // Optimization for loop code
                vRamB2 <<= 1;

                // Calculate where on the line to start to draw
                u32 writeX = (u32) (((i * 8) - scrollX) & 0xFF);
                u32 depth = 1;
                if(priority) {
                    depth = 3;
                }

                for(int x = 0; x < 8; x++ , writeX = (writeX + 1) & 0xFF) {
                    if(writeX >= 160) {
                        continue;
                    }

                    u32 colorid = ((vRamB1 >> x) & 0x01) | ((vRamB2 >> x) & 0x02);
                    if(gameboy->sgbMode) {
                        paletteid = subSgbMap[writeX >> 3];
                    }

                    depthBuffer[writeX] = depth - (!priority && colorid == 0);
                    lineBuffer[writeX] = *bgPalettesRef[paletteid][colorid];
                }
            }
        }

        // Draw window
        if(drawingWindow) {
            int tileY = (scanline - winY) / 8;
            int endTile = 21 - winX / 8;
            for(int i = 0; i < endTile; i++) {
                int mapAddr = winMapAddr + i + (tileY * 32);

                // This is the tile id.
                int tileNum = gameboy->vram[0][mapAddr];
                if(tileSigned) {
                    tileNum = ((s8) tileNum) + 128 + 0x80;
                }

                int flipX = 0, flipY = 0;
                int bank = 0;
                int paletteid = 0;
                int priority = 0;

                if(gameboy->gbMode == CGB) {
                    flipX = gameboy->vram[1][mapAddr] & 0x20;
                    flipY = gameboy->vram[1][mapAddr] & 0x40;
                    bank = (gameboy->vram[1][mapAddr] & 0x8) != 0;
                    paletteid = gameboy->vram[1][mapAddr] & 0x7;
                    priority = gameboy->vram[1][mapAddr] & 0x80;
                }

                int pixelY = (scanline - winY) & 0x07;
                if(flipY) {
                    pixelY = 7 - pixelY;
                }

                // Read bytes of tile line
                u32 vRamB1 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1)];
                u32 vRamB2 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1) + 1];

                // Reverse their bits if flipX set
                if (!flipX) {
                    vRamB1 = BitReverseTable256[vRamB1];
                    vRamB2 = BitReverseTable256[vRamB2];
                }

                // Optimization for loop code
                vRamB2 <<= 1;

                // Calculate where on the line to start to draw
                int writeX = ((i * 8) + winX);
                u32 depth = 1;
                if(priority) {
                    depth = 3;
                }

                for(int x = 0; x < 8; x++ , writeX++) {
                    if(writeX >= 160) {
                        continue;
                    }

                    u32 colorid = ((vRamB1 >> x) & 0x01) | ((vRamB2 >> x) & 0x02);
                    if(gameboy->sgbMode) {
                        paletteid = subSgbMap[writeX>>3];
                    }

                    depthBuffer[writeX] = depth - (!priority && colorid == 0);
                    lineBuffer[writeX] = *bgPalettesRef[paletteid][colorid];
                }
            }
        }

        if(gameboy->ioRam[0x40] & 0x2) { // Sprites enabled
            for(int i = 39; i >= 0; i--) {
                drawSprite(scanline, i);
            }
        }
    }
}

void GameboyPPU::drawSprite(int scanline, int spriteNum) {
    // The sprite's number, times 4 (each uses 4 bytes)
    spriteNum *= 4;

    int y = (gameboy->hram[spriteNum] - 16);
    int height;
    if(gameboy->ioRam[0x40] & 0x4) {
        height = 16;
    } else {
        height = 8;
    }

    if(scanline < y || scanline >= y + height) {
        return;
    }

    int x = (gameboy->hram[spriteNum + 1] - 8);
    int tileNum = gameboy->hram[spriteNum + 2];
    int bank = 0;
    int flipX = (gameboy->hram[spriteNum + 3] & 0x20);
    int flipY = (gameboy->hram[spriteNum + 3] & 0x40);
    int priority = !(gameboy->hram[spriteNum + 3] & 0x80);
    int paletteid;
    if(gameboy->gbMode == CGB) {
        bank = (gameboy->hram[spriteNum + 3] & 0x8) >> 3;
        paletteid = gameboy->hram[spriteNum + 3] & 0x7;
    } else {
        paletteid = ((gameboy->hram[spriteNum + 3] & 0x10) >> 4) * 4;
    }

    if(height == 16) {
        tileNum &= ~1;

        if(scanline - y >= 8) {
            tileNum++;
        }
    }

    int pixelY = (scanline - y) % 8;
    if(flipY) {
        pixelY = 7 - pixelY;

        if(height == 16) {
            tileNum = tileNum ^ 1;
        }
    }

    u32 depth = 0;
    if(priority) {
        depth = 2;
    }

    // Read bytes of tile line
    u32 vRamB1 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1)];
    u32 vRamB2 = gameboy->vram[bank][(tileNum << 4) + (pixelY << 1) + 1];

    // Reverse their bits if flipX set
    if (!flipX) {
        vRamB1 = BitReverseTable256[vRamB1];
        vRamB2 = BitReverseTable256[vRamB2];
    }

    // Optimization for loop code
    vRamB2 <<= 1;
    int writeX = x & 0xFF;
    for(int j = 0; j < 8; j++ , writeX = (writeX + 1) & 0xFF) {
        if(writeX >= 160) {
            continue;
        }

        int palette = paletteid;
        if(gameboy->sgbMode) {
            palette += subSgbMap[writeX >> 3];
        }

        u32 colorid = ((vRamB1 >> j) & 0x01) | ((vRamB2 >> j) & 0x02);
        if(colorid != 0 && depth >= depthBuffer[writeX]) {
            depthBuffer[writeX] = depth;
            lineBuffer[writeX] = *sprPalettesRef[palette][colorid];
        }
    }
}

void GameboyPPU::drawScreen() {
    screenWasDisabled = false;
    if(!gameboy->getRomFile()->isGBS()) {
        gfxDrawScreen();
    } else if(!gfxGetFastForward() && !inputKeyHeld(inputMapFuncKey(FUNC_KEY_FAST_FORWARD))) {
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
                memset(gfxGetScreenBuffer(), 0xFF, 256 * 256 * sizeof(u32));
            } else if (!(gameboy->ioRam[ioReg] & 0x80) && (val & 0x80)) {
                screenWasDisabled = true;
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

