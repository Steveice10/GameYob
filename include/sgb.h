#pragma once

#include <istream>
#include <ostream>

#include "types.h"

class SGB {
public:
    SGB(Gameboy* gameboy);

    void reset();

    void loadState(std::istream& data, u8 version);
    void saveState(std::ostream& data);

    void update();

    inline u8 getGfxMask() {
        return this->mask;
    }

    u8* getPaletteMap() {
        return this->paletteMap;
    }

    inline void setController(u8 controller, u8 val) {
        this->controllers[controller] = val;
    }
private:
    void refreshBg();

    void loadAttrFile(int index);

    // Begin commands
    void palXX(int block);
    void attrBlock(int block);
    void attrLin(int block);
    void attrDiv(int block);
    void attrChr(int block);
    void sound(int block);
    void souTrn(int block);
    void palSet(int block);
    void palTrn(int block);
    void atrcEn(int block);
    void testEn(int block);
    void iconEn(int block);
    void dataSnd(int block);
    void dataTrn(int block);
    void mltReq(int block);
    void jump(int block);
    void chrTrn(int block);
    void pctTrn(int block);
    void attrTrn(int block);
    void attrSet(int block);
    void maskEn(int block);
    void objTrn(int block);
    // End commands

    void (SGB::*sgbCommands[32])(int) = {
            &SGB::palXX, &SGB::palXX, &SGB::palXX, &SGB::palXX, &SGB::attrBlock,
            &SGB::attrLin, &SGB::attrDiv, &SGB::attrChr,
            &SGB::sound, &SGB::souTrn, &SGB::palSet, &SGB::palTrn, &SGB::atrcEn,
            &SGB::testEn, &SGB::iconEn, &SGB::dataSnd,
            &SGB::dataTrn, &SGB::mltReq, &SGB::jump, &SGB::chrTrn, &SGB::pctTrn,
            &SGB::attrTrn, &SGB::attrSet, &SGB::maskEn,
            &SGB::objTrn, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };

    Gameboy* gameboy;

    s32 packetLength; // Number of packets to be transferred this command
    s32 packetsTransferred; // Number of packets which have been transferred so far
    s32 packetBit; // Next bit # to be sent in the packet. -1 if no packet is being transferred.
    u8 packet[16];
    u8 command;

    // Data for various different sgb commands
    struct SgbCmdData {
        int numDataSets;

        union {
            struct {
                u8 data[6];
                u8 dataBytes;
            } attrBlock;

            struct {
                u8 writeStyle;
                u8 x, y;
            } attrChr;
        };
    } cmdData;

    u8 controllers[4];
    u8 numControllers;
    u8 selectedController; // Which controller is being observed
    u8 buttonsChecked;

    u8 palettes[0x1000];
    u8 attrFiles[0x1000];

    bool hasBg;
    u8 bgTiles[0x2000];
    u8 bgMap[0x1000];

    u8 mask;
    u8 paletteMap[20 * 18];
};