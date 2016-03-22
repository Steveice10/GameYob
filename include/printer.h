#pragma once

#include <istream>
#include <ostream>

#include "types.h"

#define PRINTER_WIDTH 160
#define PRINTER_HEIGHT 208 // The actual value is 200, but 16 divides 208.

class Gameboy;

class Printer {
public:
    Printer(Gameboy* gb);

    void reset();

    void loadState(std::istream& data, u8 version);
    void saveState(std::ostream& data);

    void update();

    u8 link(u8 val);
private:
    void processBodyData(u8 dat);
    void saveImage();

    Gameboy* gameboy;

    u8 gfx[PRINTER_WIDTH * PRINTER_HEIGHT / 4];
    int gfxIndex;

    int packetByte;
    u8 status;
    u8 cmd;
    u16 cmdLength;

    bool packetCompressed;
    u8 compressionByte;
    u8 compressionLen;

    u16 expectedChecksum;
    u16 checksum;

    u8 cmd2Index;
    int margins;
    int lastMargins; // it's an int so that it can have a "nonexistant" value ("never set").
    u8 palette;
    u8 exposure; // Ignored

    int numPrinted; // Corresponds to the number after the filename

    u64 nextUpdateCycle;
};
