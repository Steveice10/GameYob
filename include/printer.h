#pragma once

#include "types.h"

#define PRINTER_WIDTH 160
#define PRINTER_HEIGHT 208 // The actual value is 200, but 16 divides 208.

class Gameboy;

class GameboyPrinter {
public:
    GameboyPrinter(Gameboy* gb);

    void initGbPrinter();
    u8 sendGbPrinterByte(u8 dat);
    void updateGbPrinter(); // Called each vblank

private:
    // Local functions
    void resetGbPrinter();
    void printerSendVariableLenData(u8 dat);
    void printerSaveFile();

    // Local variables
    Gameboy* gameboy;

    u8 printerGfx[PRINTER_WIDTH * PRINTER_HEIGHT / 4];
    int printerGfxIndex;

    int printerPacketByte;
    u8 printerStatus;
    u8 printerCmd;
    u16 printerCmdLength;

    bool printerPacketCompressed;
    u8 printerCompressionByte;
    u8 printerCompressionLen;

    u16 printerExpectedChecksum;
    u16 printerChecksum;

    int printerMargins;
    int lastPrinterMargins; // it's an int so that it can have a "nonexistant" value ("never set").
    u8 printerCmd2Index;
    u8 printerPalette;
    u8 printerExposure; // Ignored

    int numPrinted; // Corresponds to the number after the filename

    int printCounter = 0; // Timer until the printer "stops printing".
};
