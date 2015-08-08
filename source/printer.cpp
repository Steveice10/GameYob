#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "platform/system.h"
#include "gameboy.h"
#include "printer.h"
#include "romfile.h"

#define PRINTER_STATUS_READY        0x08
#define PRINTER_STATUS_REQUESTED    0x04
#define PRINTER_STATUS_PRINTING     0x02
#define PRINTER_STATUS_CHECKSUM     0x01

GameboyPrinter::GameboyPrinter(Gameboy* gb) {
    this->gameboy = gb;
}

// Called along with other initialization routines
void GameboyPrinter::initGbPrinter() {
    printerPacketByte = 0;
    printerChecksum = 0;
    printerCmd2Index = 0;

    printerMargins = -1;
    lastPrinterMargins = -1;

    numPrinted = 0;

    resetGbPrinter();
}

u8 GameboyPrinter::sendGbPrinterByte(u8 dat) {
    u8 linkReceivedData = 0x00;

    // "Byte" 6 is actually a number of bytes. The counter stays at 6 until the
    // required number of bytes have been read.
    if(printerPacketByte == 6 && printerCmdLength == 0) {
        printerPacketByte++;
    }

    // Checksum: don't count the magic bytes or checksum bytes
    if(printerPacketByte != 0 && printerPacketByte != 1 && printerPacketByte != 7 && printerPacketByte != 8) {
        printerChecksum += dat;
    }

    switch(printerPacketByte) {
        case 0: // Magic byte
            linkReceivedData = 0x00;
            if(dat != 0x88) {
                goto endPacket;
            }

            break;
        case 1: // Magic byte
            linkReceivedData = 0x00;
            if(dat != 0x33) {
                goto endPacket;
            }

            break;
        case 2: // Command
            linkReceivedData = 0x00;
            printerCmd = dat;
            break;
        case 3: // Compression flag
            linkReceivedData = 0x00;
            printerPacketCompressed = dat;
            if(printerPacketCompressed) {
                printerCompressionLen = 0;
            }

            break;
        case 4: // Length (LSB)
            linkReceivedData = 0x00;
            printerCmdLength = dat;
            break;
        case 5: // Length (MSB)
            linkReceivedData = 0x00;
            printerCmdLength |= dat << 8;
            break;
        case 6: // variable-length data
            linkReceivedData = 0x00;

            if(!printerPacketCompressed) {
                printerSendVariableLenData(dat);
            } else {
                // Handle RLE compression
                if(printerCompressionLen == 0) {
                    printerCompressionByte = dat;
                    printerCompressionLen = (dat & 0x7f) + 1;
                    if(printerCompressionByte & 0x80) {
                        printerCompressionLen++;
                    }
                } else {
                    if(printerCompressionByte & 0x80) {
                        while(printerCompressionLen != 0) {
                            printerSendVariableLenData(dat);
                            printerCompressionLen--;
                        }
                    } else {
                        printerSendVariableLenData(dat);
                        printerCompressionLen--;
                    }
                }
            }

            printerCmdLength--;
            return linkReceivedData; // printerPacketByte won't be incremented
        case 7: // Checksum (LSB)
            linkReceivedData = 0x00;
            printerExpectedChecksum = dat;
            break;
        case 8: // Checksum (MSB)
            linkReceivedData = 0x00;
            printerExpectedChecksum |= dat << 8;
            break;
        case 9: // Alive indicator
            linkReceivedData = 0x81;
            break;
        case 10: // Status
            if(printerChecksum != printerExpectedChecksum) {
                printerStatus |= PRINTER_STATUS_CHECKSUM;
                systemPrintDebug("Checksum %.4x, expected %.4x\n", printerChecksum, printerExpectedChecksum);
            } else {
                printerStatus &= ~PRINTER_STATUS_CHECKSUM;
            }

            switch(printerCmd) {
                case 1: // Initialize
                    resetGbPrinter();
                    break;
                case 2: // Start printing (after a short delay)
                    printCounter = 1;
                    break;
                case 4: // Fill buffer
                    // Data has been read, nothing more to do
                    break;
            }

            linkReceivedData = printerStatus;

            // The received value apparently shouldn't contain this until next packet.
            if(printerGfxIndex >= 0x280) {
                printerStatus |= PRINTER_STATUS_READY;
            }

            goto endPacket;
    }

    printerPacketByte++;
    return linkReceivedData;

    endPacket:
    printerPacketByte = 0;
    printerChecksum = 0;
    printerCmd2Index = 0;
    return linkReceivedData;
}

void GameboyPrinter::updateGbPrinter() {
    if(printCounter != 0) {
        printCounter--;
        if(printCounter == 0) {
            if(printerStatus & PRINTER_STATUS_PRINTING) {
                printerStatus &= ~PRINTER_STATUS_PRINTING;
            } else {
                printerStatus |= PRINTER_STATUS_REQUESTED;
                printerStatus |= PRINTER_STATUS_PRINTING;
                printerStatus &= ~PRINTER_STATUS_READY;
                printerSaveFile();
            }
        }
    }
}

// Can be invoked by the game (command 1)
void GameboyPrinter::resetGbPrinter() {
    printerStatus = 0;
    printerGfxIndex = 0;
    memset(printerGfx, 0, sizeof(printerGfx));
    printCounter = 0;
}

void GameboyPrinter::printerSendVariableLenData(u8 dat) {
    switch(printerCmd) {
        case 0x2: // Print
            switch(printerCmd2Index) {
                case 0: // Unknown (0x01)
                    break;
                case 1: // Margins
                    lastPrinterMargins = printerMargins;
                    printerMargins = dat;
                    break;
                case 2: // Palette
                    printerPalette = dat;
                    break;
                case 3: // Exposure / brightness
                    printerExposure = dat;
                    break;
            }

            printerCmd2Index++;
            break;
        case 0x4: // Fill buffer
            if(printerGfxIndex < PRINTER_WIDTH * PRINTER_HEIGHT / 4) {
                printerGfx[printerGfxIndex++] = dat;
            }

            break;
    }
}

// Save the image as a 4bpp bitmap
void GameboyPrinter::printerSaveFile() {
    // if "appending" is true, this image will be slapped onto the old one.
    // Some games have a tendency to print an image in multiple goes.
    bool appending = false;
    if(lastPrinterMargins != -1 && (lastPrinterMargins & 0x0f) == 0 && (printerMargins & 0xf0) == 0) {
        appending = true;
    }

    // Find the first available "print number".
    char filename[300];
    while(true) {
        snprintf(filename, 300, "%s-%d.bmp", gameboy->getRomFile()->getFileName().c_str(), numPrinted);

        // If appending, the last file written to is already selected.
        // Else, if the file doesn't exist, we're done searching.
        if(appending || access(filename, R_OK) != 0) {
            if(appending && access(filename, R_OK) != 0) {
                // This is a failsafe, this shouldn't happen
                appending = false;
                systemPrintDebug("The image to be appended to doesn't exist!");
                continue;
            } else {
                break;
            }
        }

        numPrinted++;
    }

    int width = PRINTER_WIDTH;

    // In case of error, size must be rounded off to the nearest 16 vertical pixels.
    if(printerGfxIndex % (width / 4 * 16) != 0) {
        printerGfxIndex += (width / 4 * 16) - (printerGfxIndex % (width / 4 * 16));
    }

    int height = printerGfxIndex / width * 4;
    int pixelArraySize = (width * height + 1) / 2;

    u8 bmpHeader[] = { // Contains header data & palettes
            0x42, 0x4d, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x28, 0x00,
            0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x04, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa,
            0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff
    };

    // Set up the palette
    for(int i = 0; i < 4; i++) {
        u8 rgb;
        switch((printerPalette >> (i * 2)) & 3) {
            case 0:
                rgb = 0xff;
                break;
            case 1:
                rgb = 0xaa;
                break;
            case 2:
                rgb = 0x55;
                break;
            case 3:
                rgb = 0x00;
                break;
        }

        for(int j = 0; j < 4; j++) {
            bmpHeader[0x36 + i * 4 + j] = rgb;
        }
    }

    u16* pixelData = (u16*) malloc(pixelArraySize);

    // Convert the gameboy's tile-based 2bpp into a linear 4bpp format.
    for(int i = 0; i < printerGfxIndex; i += 2) {
        u8 b1 = printerGfx[i];
        u8 b2 = printerGfx[i + 1];

        int pixel = i * 4;
        int tile = pixel / 64;

        int index = tile / 20 * width * 8;
        index += (tile % 20) * 8;
        index += ((pixel % 64) / 8) * width;
        index += (pixel % 8);
        index /= 4;

        pixelData[index] = 0;
        pixelData[index + 1] = 0;
        for(int j = 0; j < 2; j++) {
            pixelData[index] |= (((b1 >> j >> 4) & 1) | (((b2 >> j >> 4) & 1) << 1)) << (j * 4 + 8);
            pixelData[index] |= (((b1 >> j >> 6) & 1) | (((b2 >> j >> 6) & 1) << 1)) << (j * 4);
            pixelData[index + 1] |= (((b1 >> j) & 1) | (((b2 >> j) & 1) << 1)) << (j * 4 + 8);
            pixelData[index + 1] |= (((b1 >> j >> 2) & 1) | (((b2 >> j >> 2) & 1) << 1)) << (j * 4);
        }
    }

    FILE* file;
    if(appending) {
        file = fopen(filename, "r+b");
        int temp;

        // Update height
        fseek(file, 0x16, SEEK_SET);
        fread(&temp, 4, 1, file);
        temp = -(height + (-temp));
        fseek(file, 0x16, SEEK_SET);
        fwrite(&temp, 4, 1, file);

        // Update pixelArraySize
        fseek(file, 0x22, SEEK_SET);
        fread(&temp, 4, 1, file);
        temp += pixelArraySize;
        fseek(file, 0x22, SEEK_SET);
        fwrite(&temp, 4, 1, file);

        // Update file size
        temp += sizeof(bmpHeader);
        fseek(file, 0x2, SEEK_SET);
        fwrite(&temp, 4, 1, file);

        fclose(file);
        file = fopen(filename, "ab");
    } else { // Not appending; making a file from scratch
        file = fopen(filename, "ab");

        *(u32*) (bmpHeader + 0x02) = sizeof(bmpHeader) + pixelArraySize;
        *(u32*) (bmpHeader + 0x22) = (u32) pixelArraySize;
        *(u32*) (bmpHeader + 0x12) = (u32) width;
        *(u32*) (bmpHeader + 0x16) = (u32) -height;
        fwrite(bmpHeader, 1, sizeof(bmpHeader), file);
    }

    fwrite(pixelData, 1, pixelArraySize, file);

    fclose(file);

    free(pixelData);
    printerGfxIndex = 0;

    printCounter = height; // PRINTER_STATUS_PRINTING will be unset after this many frames
    if(printCounter == 0) {
        printCounter = 1;
    }
}
