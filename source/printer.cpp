#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "platform/common/manager.h"
#include "platform/system.h"
#include "gameboy.h"
#include "printer.h"
#include "romfile.h"

#define PRINTER_STATUS_READY        0x08
#define PRINTER_STATUS_REQUESTED    0x04
#define PRINTER_STATUS_PRINTING     0x02
#define PRINTER_STATUS_CHECKSUM     0x01

Printer::Printer(Gameboy* gb) {
    this->gameboy = gb;
}

void Printer::reset() {
    memset(this->gfx, 0, sizeof(this->gfx));
    this->gfxIndex = 0;

    this->packetByte = 0;
    this->status = 0;
    this->cmd = 0;
    this->cmdLength = 0;

    this->packetCompressed = false;
    this->compressionByte = 0;
    this->compressionLen = 0;

    this->expectedChecksum = 0;
    this->checksum = 0;

    this->margins = -1;
    this->lastMargins = -1;
    this->cmd2Index = 0;
    this->palette = 0;
    this->exposure = 0;

    this->numPrinted = 0;

    this->counter = 0;
}

void Printer::loadState(FILE* file, int version) {
    fread(this->gfx, 1, sizeof(this->gfx), file);
    fread(&this->gfxIndex, 1, sizeof(this->gfxIndex), file);
    fread(&this->packetByte, 1, sizeof(this->packetByte), file);
    fread(&this->status, 1, sizeof(this->status), file);
    fread(&this->cmd, 1, sizeof(this->cmd), file);
    fread(&this->cmdLength, 1, sizeof(this->cmdLength), file);
    fread(&this->packetCompressed, 1, sizeof(this->packetCompressed), file);
    fread(&this->compressionByte, 1, sizeof(this->compressionByte), file);
    fread(&this->compressionLen, 1, sizeof(this->compressionLen), file);
    fread(&this->expectedChecksum, 1, sizeof(this->expectedChecksum), file);
    fread(&this->checksum, 1, sizeof(this->checksum), file);
    fread(&this->margins, 1, sizeof(this->margins), file);
    fread(&this->lastMargins, 1, sizeof(this->lastMargins), file);
    fread(&this->cmd2Index, 1, sizeof(this->cmd2Index), file);
    fread(&this->palette, 1, sizeof(this->palette), file);
    fread(&this->exposure, 1, sizeof(this->exposure), file);
    fread(&this->numPrinted, 1, sizeof(this->numPrinted), file);
    fread(&this->counter, 1, sizeof(this->counter), file);
}

void Printer::saveState(FILE* file) {
    fwrite(this->gfx, 1, sizeof(this->gfx), file);
    fwrite(&this->gfxIndex, 1, sizeof(this->gfxIndex), file);
    fwrite(&this->packetByte, 1, sizeof(this->packetByte), file);
    fwrite(&this->status, 1, sizeof(this->status), file);
    fwrite(&this->cmd, 1, sizeof(this->cmd), file);
    fwrite(&this->cmdLength, 1, sizeof(this->cmdLength), file);
    fwrite(&this->packetCompressed, 1, sizeof(this->packetCompressed), file);
    fwrite(&this->compressionByte, 1, sizeof(this->compressionByte), file);
    fwrite(&this->compressionLen, 1, sizeof(this->compressionLen), file);
    fwrite(&this->expectedChecksum, 1, sizeof(this->expectedChecksum), file);
    fwrite(&this->checksum, 1, sizeof(this->checksum), file);
    fwrite(&this->margins, 1, sizeof(this->margins), file);
    fwrite(&this->lastMargins, 1, sizeof(this->lastMargins), file);
    fwrite(&this->cmd2Index, 1, sizeof(this->cmd2Index), file);
    fwrite(&this->palette, 1, sizeof(this->palette), file);
    fwrite(&this->exposure, 1, sizeof(this->exposure), file);
    fwrite(&this->numPrinted, 1, sizeof(this->numPrinted), file);
    fwrite(&this->counter, 1, sizeof(this->counter), file);
}

u8 Printer::link(u8 val) {
    u8 linkReceivedData = 0x00;

    // "Byte" 6 is actually a number of bytes. The counter stays at 6 until the
    // required number of bytes have been read.
    if(this->packetByte == 6 && this->cmdLength == 0) {
        this->packetByte++;
    }

    // Checksum: don't count the magic bytes or checksum bytes
    if(this->packetByte != 0 && this->packetByte != 1 && this->packetByte != 7 && this->packetByte != 8) {
        this->checksum += val;
    }

    switch(this->packetByte) {
        case 0: // Magic byte
            linkReceivedData = 0x00;
            if(val != 0x88) {
                this->packetByte = 0;
                this->checksum = 0;
                this->cmd2Index = 0;
                return linkReceivedData;
            }

            break;
        case 1: // Magic byte
            linkReceivedData = 0x00;
            if(val != 0x33) {
                this->packetByte = 0;
                this->checksum = 0;
                this->cmd2Index = 0;
                return linkReceivedData;
            }

            break;
        case 2: // Command
            linkReceivedData = 0x00;
            this->cmd = val;
            break;
        case 3: // Compression flag
            linkReceivedData = 0x00;
            this->packetCompressed = val;
            if(this->packetCompressed) {
                this->compressionLen = 0;
            }

            break;
        case 4: // Length (LSB)
            linkReceivedData = 0x00;
            this->cmdLength = val;
            break;
        case 5: // Length (MSB)
            linkReceivedData = 0x00;
            this->cmdLength |= val << 8;
            break;
        case 6: // variable-length data
            linkReceivedData = 0x00;

            if(!this->packetCompressed) {
                this->sendVariableLenData(val);
            } else {
                // Handle RLE compression
                if(this->compressionLen == 0) {
                    this->compressionByte = val;
                    this->compressionLen = (u8) ((val & 0x7f) + 1);
                    if(this->compressionByte & 0x80) {
                        this->compressionLen++;
                    }
                } else {
                    if(this->compressionByte & 0x80) {
                        while(this->compressionLen != 0) {
                            this->sendVariableLenData(val);
                            this->compressionLen--;
                        }
                    } else {
                        this->sendVariableLenData(val);
                        this->compressionLen--;
                    }
                }
            }

            this->cmdLength--;
            return linkReceivedData; // packetByte won't be incremented
        case 7: // Checksum (LSB)
            linkReceivedData = 0x00;
            this->expectedChecksum = val;
            break;
        case 8: // Checksum (MSB)
            linkReceivedData = 0x00;
            this->expectedChecksum |= val << 8;
            break;
        case 9: // Alive indicator
            linkReceivedData = 0x81;
            break;
        case 10: // Status
            if(this->checksum != this->expectedChecksum) {
                this->status |= PRINTER_STATUS_CHECKSUM;
                systemPrintDebug("Checksum %.4x, expected %.4x\n", this->checksum, this->expectedChecksum);
            } else {
                this->status &= ~PRINTER_STATUS_CHECKSUM;
            }

            switch(this->cmd) {
                case 1: // Initialize
                    this->counter = 0;
                    this->status = 0;
                    this->gfxIndex = 0;
                    memset(this->gfx, 0, sizeof(this->gfx));
                    break;
                case 2: // Start printing (after a short delay)
                    this->counter = 1;
                    break;
                case 4: // Fill buffer
                    // Data has been read, nothing more to do
                    break;
                default:
                    break;
            }

            linkReceivedData = this->status;

            // The received value apparently shouldn't contain this until next packet.
            if(this->gfxIndex >= 0x280) {
                this->status |= PRINTER_STATUS_READY;
            }

            this->packetByte = 0;
            this->checksum = 0;
            this->cmd2Index = 0;
            return linkReceivedData;
        default:
            break;
    }

    this->packetByte++;
    return linkReceivedData;
}

void Printer::update() {
    if(this->counter != 0) {
        this->counter--;
        if(this->counter == 0) {
            if(this->status & PRINTER_STATUS_PRINTING) {
                this->status &= ~PRINTER_STATUS_PRINTING;
            } else {
                this->status |= PRINTER_STATUS_REQUESTED;
                this->status |= PRINTER_STATUS_PRINTING;
                this->status &= ~PRINTER_STATUS_READY;
                saveImage();
            }
        }
    }
}

void Printer::sendVariableLenData(u8 dat) {
    switch(this->cmd) {
        case 0x2: // Print
            switch(this->cmd2Index) {
                case 0: // Unknown (0x01)
                    break;
                case 1: // Margins
                    this->lastMargins = this->margins;
                    this->margins = dat;
                    break;
                case 2: // Palette
                    this->palette = dat;
                    break;
                case 3: // Exposure / brightness
                    this->exposure = dat;
                    break;
                default:
                    break;
            }

            this->cmd2Index++;
            break;
        case 0x4: // Fill buffer
            if(this->gfxIndex < PRINTER_WIDTH * PRINTER_HEIGHT / 4) {
                this->gfx[this->gfxIndex++] = dat;
            }

            break;
        default:
            break;
    }
}

// Save the image as a 4bpp bitmap
void Printer::saveImage() {
    // if "appending" is true, this image will be slapped onto the old one.
    // Some games have a tendency to print an image in multiple goes.
    bool appending = false;
    if(this->lastMargins != -1 && (this->lastMargins & 0x0F) == 0 && (this->margins & 0xF0) == 0) {
        appending = true;
    }

    // Find the first available "print number".
    char filename[300];
    while(true) {
        snprintf(filename, 300, "%s-%d.bmp", mgrGetRomName().c_str(), this->numPrinted);

        // If appending, the last file written to is already selected.
        // Else, if the file doesn't exist, we're done searching.
        if(appending || access(filename, R_OK) != 0) {
            if(appending && access(filename, R_OK) != 0) {
                // This is a failsafe, this shouldn't happen
                appending = false;
                systemPrintDebug("The image to be appended to doesn't exist!\n");
                continue;
            } else {
                break;
            }
        }

        this->numPrinted++;
    }

    int width = PRINTER_WIDTH;

    // In case of error, size must be rounded off to the nearest 16 vertical pixels.
    if(this->gfxIndex % (width / 4 * 16) != 0) {
        this->gfxIndex += (width / 4 * 16) - (this->gfxIndex % (width / 4 * 16));
    }

    int height = this->gfxIndex / width * 4;
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
        u8 rgb = 0;
        switch((this->palette >> (i * 2)) & 3) {
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
            default:
                break;
        }

        for(int j = 0; j < 4; j++) {
            bmpHeader[0x36 + i * 4 + j] = rgb;
        }
    }

    u16* pixelData = (u16*) malloc((size_t) pixelArraySize);

    // Convert the gameboy's tile-based 2bpp into a linear 4bpp format.
    for(int i = 0; i < this->gfxIndex; i += 2) {
        u8 b1 = this->gfx[i];
        u8 b2 = this->gfx[i + 1];

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

    fwrite(pixelData, 1, (size_t) pixelArraySize, file);

    fclose(file);

    free(pixelData);
    this->gfxIndex = 0;

    this->counter = height; // PRINTER_STATUS_PRINTING will be unset after this many frames
    if(this->counter == 0) {
        this->counter = 1;
    }
}
