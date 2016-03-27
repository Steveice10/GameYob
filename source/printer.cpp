#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fstream>

#include "platform/common/manager.h"
#include "platform/system.h"
#include "cpu.h"
#include "gameboy.h"
#include "printer.h"

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

    this->nextUpdateCycle = 0;
}

void Printer::loadState(std::istream& data, u8 version) {
    data.read((char*) this->gfx, sizeof(this->gfx));
    data.read((char*) &this->gfxIndex, sizeof(this->gfxIndex));
    data.read((char*) &this->packetByte, sizeof(this->packetByte));
    data.read((char*) &this->status, sizeof(this->status));
    data.read((char*) &this->cmd, sizeof(this->cmd));
    data.read((char*) &this->cmdLength, sizeof(this->cmdLength));
    data.read((char*) &this->packetCompressed, sizeof(this->packetCompressed));
    data.read((char*) &this->compressionByte, sizeof(this->compressionByte));
    data.read((char*) &this->compressionLen, sizeof(this->compressionLen));
    data.read((char*) &this->expectedChecksum, sizeof(this->expectedChecksum));
    data.read((char*) &this->checksum, sizeof(this->checksum));
    data.read((char*) &this->cmd2Index, sizeof(this->cmd2Index));
    data.read((char*) &this->margins, sizeof(this->margins));
    data.read((char*) &this->lastMargins, sizeof(this->lastMargins));
    data.read((char*) &this->palette, sizeof(this->palette));
    data.read((char*) &this->exposure, sizeof(this->exposure));
    data.read((char*) &this->numPrinted, sizeof(this->numPrinted));
    data.read((char*) &this->nextUpdateCycle, sizeof(this->nextUpdateCycle));
}

void Printer::saveState(std::ostream& data) {
    data.write((char*) this->gfx, sizeof(this->gfx));
    data.write((char*) &this->gfxIndex, sizeof(this->gfxIndex));
    data.write((char*) &this->packetByte, sizeof(this->packetByte));
    data.write((char*) &this->status, sizeof(this->status));
    data.write((char*) &this->cmd, sizeof(this->cmd));
    data.write((char*) &this->cmdLength, sizeof(this->cmdLength));
    data.write((char*) &this->packetCompressed, sizeof(this->packetCompressed));
    data.write((char*) &this->compressionByte, sizeof(this->compressionByte));
    data.write((char*) &this->compressionLen, sizeof(this->compressionLen));
    data.write((char*) &this->expectedChecksum, sizeof(this->expectedChecksum));
    data.write((char*) &this->checksum, sizeof(this->checksum));
    data.write((char*) &this->cmd2Index, sizeof(this->cmd2Index));
    data.write((char*) &this->margins, sizeof(this->margins));
    data.write((char*) &this->lastMargins, sizeof(this->lastMargins));
    data.write((char*) &this->palette, sizeof(this->palette));
    data.write((char*) &this->exposure, sizeof(this->exposure));
    data.write((char*) &this->numPrinted, sizeof(this->numPrinted));
    data.write((char*) &this->nextUpdateCycle, sizeof(this->nextUpdateCycle));
}

u8 Printer::link(u8 val) {
    // "Byte" 6 is actually a number of bytes. The counter stays at 6 until the
    // required number of bytes have been read.
    if(this->packetByte == 6 && this->cmdLength == 0) {
        this->packetByte++;
    }

    // Checksum: don't count the magic bytes or checksum bytes
    if(this->packetByte != 0 && this->packetByte != 1 && this->packetByte != 7 && this->packetByte != 8) {
        this->checksum += val;
    }

    switch(this->packetByte++) {
        case 0: // Magic byte
            if(val != 0x88) {
                this->packetByte = 0;
                this->checksum = 0;
                this->cmd2Index = 0;
            }

            return 0;
        case 1: // Magic byte
            if(val != 0x33) {
                this->packetByte = 0;
                this->checksum = 0;
                this->cmd2Index = 0;
            }

            return 0;
        case 2: // Command
            this->cmd = val;
            return 0;
        case 3: // Compression flag
            this->packetCompressed = val;
            if(this->packetCompressed) {
                this->compressionLen = 0;
            }

            return 0;
        case 4: // Length (LSB)
            this->cmdLength = (u16) ((this->cmdLength & 0xFF00) | val);
            return 0;
        case 5: // Length (MSB)
            this->cmdLength = (u16) ((this->cmdLength & 0xFF) | (val << 8));
            return 0;
        case 6: // variable-length data
            if(!this->packetCompressed) {
                this->processBodyData(val);
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
                            this->processBodyData(val);
                            this->compressionLen--;
                        }
                    } else {
                        this->processBodyData(val);
                        this->compressionLen--;
                    }
                }
            }

            this->cmdLength--;
            this->packetByte--; // Remain on 6 until we've received everything.
            return 0;
        case 7: // Checksum (LSB)
            this->expectedChecksum = (u16) ((this->expectedChecksum & 0xFF00) | val);
            return 0;
        case 8: // Checksum (MSB)
            this->expectedChecksum = (u16) ((this->expectedChecksum & 0xFF) | (val << 8));
            return 0;
        case 9: // Alive indicator
            return 0x81;
        case 10: { // Status
            if(this->checksum != this->expectedChecksum) {
                this->status |= PRINTER_STATUS_CHECKSUM;
                systemPrintDebug("Printer Error: Checksum %.4x, Expected %.4x\n", this->checksum, this->expectedChecksum);
            } else {
                this->status &= ~PRINTER_STATUS_CHECKSUM;
            }

            switch(this->cmd) {
                case 1: // Initialize
                    this->nextUpdateCycle = 0;
                    this->status = 0;
                    this->gfxIndex = 0;
                    memset(this->gfx, 0, sizeof(this->gfx));
                    break;
                case 2: // Start printing (after a short delay)
                    this->nextUpdateCycle = this->gameboy->cpu->getCycle() + CYCLES_PER_FRAME;
                    this->gameboy->cpu->setEventCycle(this->nextUpdateCycle);
                    break;
                case 4: // Fill buffer
                    // Data has been read, nothing more to do
                    break;
                default:
                    break;
            }

            u8 reply = this->status;

            // The received value apparently shouldn't contain this until next packet.
            if(this->gfxIndex >= 0x280) {
                this->status |= PRINTER_STATUS_READY;
            }

            this->packetByte = 0;
            this->checksum = 0;
            this->cmd2Index = 0;
            return reply;
        }
        default:
            break;
    }

    return 0;
}

void Printer::update() {
    if(this->nextUpdateCycle != 0) {
        if(this->gameboy->cpu->getCycle() >= this->nextUpdateCycle) {
            this->nextUpdateCycle = 0;

            if(this->status & PRINTER_STATUS_PRINTING) {
                this->status &= ~PRINTER_STATUS_PRINTING;
            } else {
                this->status |= PRINTER_STATUS_REQUESTED;
                this->status |= PRINTER_STATUS_PRINTING;
                this->status &= ~PRINTER_STATUS_READY;
                this->saveImage();
            }
        } else {
            this->gameboy->cpu->setEventCycle(this->nextUpdateCycle);
        }
    }
}

void Printer::processBodyData(u8 dat) {
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

    if(appending) {
        std::fstream stream(filename, std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::ate);
        s64 end = stream.tellg();

        int temp = 0;

        // Update height
        stream.seekg(0x16);
        stream.read((char*) &temp, sizeof(temp));
        temp = -(height + (-temp));
        stream.seekg(0x16);
        stream.write((char*) &temp, sizeof(temp));

        // Update pixelArraySize
        stream.seekg(0x22);
        stream.read((char*) &temp, sizeof(temp));
        temp += pixelArraySize;
        stream.seekg(0x22);
        stream.write((char*) &temp, sizeof(temp));

        // Update file size
        temp += sizeof(bmpHeader);
        stream.seekg(0x2);
        stream.write((char*) &temp, sizeof(temp));

        // Append pixel data
        stream.seekg(end);
        stream.write((char*) pixelData, pixelArraySize);

        stream.close();
    } else { // Not appending; making a file from scratch
        std::fstream stream(filename, std::fstream::out | std::fstream::binary);

        *(u32*) (bmpHeader + 0x02) = sizeof(bmpHeader) + pixelArraySize;
        *(u32*) (bmpHeader + 0x22) = (u32) pixelArraySize;
        *(u32*) (bmpHeader + 0x12) = (u32) width;
        *(u32*) (bmpHeader + 0x16) = (u32) -height;

        stream.write((char*) bmpHeader, sizeof(bmpHeader));
        stream.write((char*) pixelData, pixelArraySize);
        stream.close();
    }

    free(pixelData);
    this->gfxIndex = 0;

    // PRINTER_STATUS_PRINTING will be unset after this many frames
    this->nextUpdateCycle = this->gameboy->cpu->getCycle() + ((height != 0 ? height : 1) * CYCLES_PER_FRAME);
    this->gameboy->cpu->setEventCycle(this->nextUpdateCycle);
}
