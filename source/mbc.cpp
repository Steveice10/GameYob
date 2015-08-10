#include <time.h>

#include "platform/input.h"
#include "platform/system.h"
#include "gameboy.h"
#include "romfile.h"

/* MBC read handlers */

/* MBC3 */
u8 Gameboy::m3r(u16 addr) {
    if(!ramEnabled) {
        return 0xff;
    }

    switch(ramBankNum) { // Check for RTC register
        case 0x8:
            return gbClock.mbc3.s;
        case 0x9:
            return gbClock.mbc3.m;
        case 0xA:
            return gbClock.mbc3.h;
        case 0xB:
            return gbClock.mbc3.d & 0xff;
        case 0xC:
            return gbClock.mbc3.ctrl;
        default: // Not an RTC register
            return ramEnabled ? memory[addr >> 12][addr & 0xfff] : 0xff;
    }
}

/* MBC7 */
u8 Gameboy::m7r(u16 addr) {
    switch(addr & 0xa0f0) {
        case 0xa000:
        case 0xa010:
        case 0xa060:
        case 0xa070:
            return 0;
        case 0xa020:
            return inputGetMotionSensorX() & 0xff;
        case 0xa030:
            return (inputGetMotionSensorX() >> 8) | 0x80;
        case 0xa040:
            return inputGetMotionSensorY() & 0xff;
        case 0xa050:
            return inputGetMotionSensorY() >> 8;
        case 0xa080:
            return mbc7RA;
        default:
            return 0xff;
    }
}

/* HUC3 */
u8 Gameboy::h3r(u16 addr) {
    switch(HuC3Mode) {
        case 0xc:
            return HuC3Value;
        case 0xb:
        case 0xd:
            /* Return 1 as a fixed value, needed for some games to
             * boot, the meaning is unknown. */
            return 1;
    }

    return (ramEnabled) ? memory[addr >> 12][addr & 0xfff] : 0xff;
}

/* CAMERA */
u8 Gameboy::camr(u16 addr) {
    if(cameraIO) {
        // 0xA000: hardware ready, Others: write only
        return addr == 0xA000 ? 0x00 : 0xFF;
    } else {
        return ramEnabled ? memory[addr >> 12][addr & 0xfff] : 0xff;
    }
}


/* MBC Write handlers */

/* MBC0 (ROM) */
void Gameboy::m0w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if(romFile->getRamBanks() > 0) {
                writeSram(addr & 0x1fff, val);
            }

            break;
    }
}

/* MBC1 */
void Gameboy::m1w(u16 addr, u8 val) {
    int newBank;

    switch(addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            val &= 0x1f;
            if(rockmanMapper) {
                newBank = ((val > 0xf) ? val - 8 : val);
            } else {
                newBank = (romBank1Num & 0xe0) | val;
            }

            refreshRomBank1((newBank) ? newBank : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            val &= 3;
            if(memoryModel == 0) { /* ROM mode */
                newBank = (romBank1Num & 0x1F) | (val << 5);
                refreshRomBank1((newBank) ? newBank : 1);
            } else { /* RAM mode */
                refreshRamBank(val);
            }

            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            memoryModel = val & 1;
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if(ramEnabled && romFile->getRamBanks() > 0) {
                writeSram(addr & 0x1fff, val);
            }

            break;
    }
}

/* MBC2 */
void Gameboy::m2w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            refreshRomBank1((val) ? val : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if(ramEnabled && romFile->getRamBanks() > 0) {
                writeSram(addr & 0x1fff, val & 0xf);
            }

            break;
    }
}

/* MBC3 */
void Gameboy::m3w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            val &= 0x7f;
            refreshRomBank1((val) ? val : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            /* The RTC register is selected by writing values 0x8-0xc, ram banks
             * are selected by values 0x0-0x3 */
            if(val <= 0x3) {
                refreshRamBank(val);
            } else if(val >= 8 && val <= 0xc) {
                ramBankNum = val;
            }

            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            if(val) {
                latchClock();
            }

            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if(!ramEnabled) {
                break;
            }

            switch(ramBankNum) { // Check for RTC register
                case 0x8:
                    if(gbClock.mbc3.s != val) {
                        gbClock.mbc3.s = val;
                        writeClockStruct();
                    }

                    return;
                case 0x9:
                    if(gbClock.mbc3.m != val) {
                        gbClock.mbc3.m = val;
                        writeClockStruct();
                    }

                    return;
                case 0xA:
                    if(gbClock.mbc3.h != val) {
                        gbClock.mbc3.h = val;
                        writeClockStruct();
                    }

                    return;
                case 0xB:
                    if((gbClock.mbc3.d & 0xff) != val) {
                        gbClock.mbc3.d &= 0x100;
                        gbClock.mbc3.d |= val;
                        writeClockStruct();
                    }

                    return;
                case 0xC:
                    if(gbClock.mbc3.ctrl != val) {
                        gbClock.mbc3.d &= 0xFF;
                        gbClock.mbc3.d |= (val & 1) << 8;
                        gbClock.mbc3.ctrl = val;
                        writeClockStruct();
                    }

                    return;
                default: // Not an RTC register
                    if(romFile->getRamBanks() > 0) {
                        writeSram(addr & 0x1fff, val);
                    }
            }

            break;
    }
}

void Gameboy::writeClockStruct() {
    if(autosaveEnabled) {
        fseek(saveFile, romFile->getRamBanks() * 0x2000, SEEK_SET);
        fwrite(&gbClock, 1, sizeof(gbClock), saveFile);
        saveModified = true;
    }
}

/* MBC5 */
void Gameboy::m5w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
            refreshRomBank1((romBank1Num & 0x100) | val);
            break;
        case 0x3:
            refreshRomBank1((romBank1Num & 0xff) | (val & 1) << 8);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            /* MBC5 might have a rumble motor, which is triggered by the
             * 4th bit of the value written */
            if(romFile->hasRumble()) {
                val &= 0x07;
            } else {
                val &= 0x0f;
            }

            refreshRamBank(val);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if(ramEnabled && romFile->getRamBanks() > 0) {
                writeSram(addr & 0x1fff, val);
            }

            break;
    }
}

/* MBC7 */
void Gameboy::m7w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xF) == 0xA);
            break;
        case 0x2: /* 2000 - 3fff */
            refreshRomBank1((romBank1Num & 0x100) | val);
            break;
        case 0x3:
            refreshRomBank1((romBank1Num & 0xFF) | (val & 1) << 8);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            refreshRamBank(val & 0xF);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if(addr == 0xA080) {
                int oldCs = mbc7Cs;
                int oldSk = mbc7Sk;

                mbc7Cs = val >> 7;
                mbc7Sk = (u8) ((val >> 6) & 1);

                if(!oldCs && mbc7Cs) {
                    if(mbc7State == 5) {
                        if(mbc7WriteEnable) {
                            writeSram((u16) (mbc7Addr * 2), (u8) (mbc7Buffer >> 8));
                            writeSram((u16) (mbc7Addr * 2 + 1), (u8) (mbc7Buffer & 0xff));
                        }

                        mbc7State = 0;
                        mbc7RA = 1;
                    } else {
                        mbc7Idle = true;
                        mbc7State = 0;
                    }
                }

                if(!oldSk && mbc7Sk) {
                    if(mbc7Idle) {
                        if(val & 0x02) {
                            mbc7Idle = false;
                            mbc7Count = 0;
                            mbc7State = 1;
                        }
                    } else {
                        switch(mbc7State) {
                            case 1:
                                mbc7Buffer <<= 1;
                                mbc7Buffer |= (val & 0x02) ? 1 : 0;
                                mbc7Count++;
                                if(mbc7Count == 2) {
                                    mbc7State = 2;
                                    mbc7Count = 0;
                                    mbc7OpCode = (u8) (mbc7Buffer & 3);
                                }
                                break;
                            case 2:
                                mbc7Buffer <<= 1;
                                mbc7Buffer |= (val & 0x02) ? 1 : 0;
                                mbc7Count++;
                                if(mbc7Count == 8) {
                                    mbc7State = 3;
                                    mbc7Count = 0;
                                    mbc7Addr = (u8) (mbc7Buffer & 0xff);
                                    if(mbc7OpCode == 0) {
                                        if((mbc7Addr >> 6) == 0) {
                                            mbc7WriteEnable = false;
                                            mbc7State = 0;
                                        } else if((mbc7Addr >> 6) == 3) {
                                            mbc7WriteEnable = true;
                                            mbc7State = 0;
                                        }
                                    }
                                }
                                break;
                            case 3:
                                mbc7Buffer <<= 1;
                                mbc7Buffer |= (val & 0x02) ? 1 : 0;
                                mbc7Count++;
                                switch(mbc7OpCode) {
                                    case 0:
                                        if(mbc7Count == 16) {
                                            if((mbc7Addr >> 6) == 0) {
                                                mbc7WriteEnable = false;
                                                mbc7State = 0;
                                            } else if((mbc7Addr >> 6) == 1) {
                                                if(mbc7WriteEnable) {
                                                    for(int i = 0; i < 256; i++) {
                                                        writeSram((u16) (i * 2), (u8) (mbc7Buffer >> 8));
                                                        writeSram((u16) (i * 2 + 1), (u8) (mbc7Buffer & 0xff));
                                                    }
                                                }

                                                mbc7State = 5;
                                            } else if((mbc7Addr >> 6) == 2) {
                                                if(mbc7WriteEnable) {
                                                    for(int i = 0; i < 256; i++) {
                                                        writeSram((u16) (i * 2), 0xff);
                                                        writeSram((u16) (i * 2 + 1), 0xff);
                                                    }
                                                }

                                                mbc7State = 5;
                                            } else if((mbc7Addr >> 6) == 3) {
                                                mbc7WriteEnable = true;
                                                mbc7State = 0;
                                            }

                                            mbc7Count = 0;
                                        }
                                        break;
                                    case 1:
                                        if(mbc7Count == 16) {
                                            mbc7Count = 0;
                                            mbc7State = 5;
                                            mbc7RA = 0;
                                        }

                                        break;
                                    case 2:
                                        if(mbc7Count == 1) {
                                            mbc7State = 4;
                                            mbc7Count = 0;
                                            mbc7Buffer = (externRam[mbc7Addr * 2] << 8) | (externRam[mbc7Addr * 2 + 1]);
                                        }

                                        break;
                                    case 3:
                                        if(mbc7Count == 16) {
                                            mbc7Count = 0;
                                            mbc7State = 5;
                                            mbc7RA = 0;
                                            mbc7Buffer = 0xffff;
                                        }

                                        break;
                                    default:
                                        break;
                                }

                                break;
                            default:
                                break;
                        }
                    }
                }

                if(oldSk && !mbc7Sk) {
                    if(mbc7State == 4) {
                        mbc7RA = (u8) ((mbc7Buffer & 0x8000) ? 1 : 0);
                        mbc7Buffer <<= 1;
                        mbc7Count++;
                        if(mbc7Count == 16) {
                            mbc7Count = 0;
                            mbc7State = 0;
                        }
                    }
                }
            }

            break;
    }
}

/* MMM01 */
void Gameboy::mmm01w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            if(mmm01BankSelected) {
                ramEnabled = (val & 0xF) == 0xA;
            } else {
                mmm01BankSelected = true;

                refreshRomBank0(mmm01RomBaseBank);
                refreshRomBank1(mmm01RomBaseBank + 1);
            }

            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            if(mmm01BankSelected) {
                refreshRomBank1(mmm01RomBaseBank + (val ? val : 1));
            } else {
                mmm01RomBaseBank = (u8) (((val & 0x3F) % romFile->getRomBanks()) + 2);
            }

            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            if(mmm01BankSelected) {
                refreshRamBank(val);
            }

            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if(mmm01BankSelected && ramEnabled && romFile->getRamBanks() > 0) {
                writeSram(addr & 0x1fff, val);
            }

            break;
    }
}

/* HUC1 */
void Gameboy::h1w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            refreshRomBank1(val & 0x3f);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            val &= 3;
            if(memoryModel == 0) {
                /* ROM mode */
                refreshRomBank1(val);
            } else {
                /* RAM mode */
                refreshRamBank(val);
            }

            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            memoryModel = val & 1;
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if(ramEnabled && romFile->getRamBanks() > 0) {
                writeSram(addr & 0x1fff, val);
            }

            break;
    }
}

/* HUC3 */
void Gameboy::h3w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            HuC3Mode = val;
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            refreshRomBank1((val) ? val : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            refreshRamBank(val & 0xf);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            switch(HuC3Mode) {
                case 0xb:
                    switch(val & 0xf0) {
                        case 0x10: /* Read clock */
                            if(HuC3Shift > 24) {
                                break;
                            }

                            switch(HuC3Shift) {
                                case 0:
                                case 4:
                                case 8:     /* Minutes */
                                    HuC3Value = (gbClock.huc3.m >> HuC3Shift) & 0xf;
                                    break;
                                case 12:
                                case 16:
                                case 20:  /* Days */
                                    HuC3Value = (gbClock.huc3.d >> (HuC3Shift - 12)) & 0xf;
                                    break;
                                case 24:                    /* Year */
                                    HuC3Value = gbClock.huc3.y & 0xf;
                                    break;
                                default:
                                    break;
                            }

                            HuC3Shift += 4;
                            break;
                        case 0x40:
                            switch(val & 0xf) {
                                case 0:
                                case 4:
                                case 7:
                                    HuC3Shift = 0;
                                    break;
                                default:
                                    break;
                            }

                            latchClock();
                            break;
                        case 0x50:
                            break;
                        case 0x60:
                            HuC3Value = 1;
                            break;
                        default:
                            systemPrintDebug("Unhandled HuC3 command 0x%02X.\n", val);
                    }

                    break;
                case 0xc:
                case 0xd:
                case 0xe:
                    break;
                default:
                    if(ramEnabled && romFile->getRamBanks() > 0) {
                        writeSram(addr & 0x1fff, val);
                    }
            }

            break;
    }
}

/* CAMERA */
void Gameboy::camw(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0:
        case 0x1:
            ramEnabled = (val & 0xF) == 0xA;
            break;
        case 0x2:
        case 0x3:
            refreshRomBank1(val ? val : 1);
            break;
        case 0x4:
        case 0x5:
            cameraIO = val == 0x10;
            if(!cameraIO) {
                refreshRamBank(val & 0xF);
            }

            break;
        case 0x6:
        case 0x7:
            break;
        case 0xA:
        case 0xB:
            if(cameraIO) {
                // TODO: Handle I/O registers?
            } else if(ramEnabled && romFile->getRamBanks() > 0) {
                writeSram(addr & 0x1fff, val);
            }

            break;
        default:
            break;
    }
}

/* TAMA5 */
void Gameboy::t5w(u16 addr, u8 val) {
    if(addr <= 0xa001) {
        switch(addr & 1) {
            case 0: {
                val &= 0xf;
                tama5Commands[tama5CommandNumber] = val;
                memory[0xA][0] = val;
                if((tama5CommandNumber & 0xE) == 0) {
                    refreshRomBank1(tama5Commands[0] | (tama5Commands[1] << 4));
                    tama5Commands[0x0F] = 0;
                } else if((tama5CommandNumber & 0xE) == 4) {
                    tama5Commands[0x0F] = 1;
                    if(tama5CommandNumber == 4) {
                        tama5Commands[0x05] = 0;
                    }
                } else if((tama5CommandNumber & 0xE) == 6) {
                    tama5RamByteSelect = (tama5Commands[7] << 4) | (tama5Commands[6] & 0x0F);
                    if(tama5Commands[0x0F] && tama5CommandNumber == 7) {
                        int data = (tama5Commands[0x04] & 0x0F) | (tama5Commands[0x05] << 4);
                        if(tama5RamByteSelect == 0x8) {
                            switch (data & 0xF) {
                                case 0x7:
                                    gbClock.tama5.d = (gbClock.tama5.d / 10) * 10 + (data >> 4);
                                    break;
                                case 0x8:
                                    gbClock.tama5.d = (gbClock.tama5.d % 10) + (data >> 4) * 10;
                                    break;
                                case 0x9:
                                    gbClock.tama5.mon = (gbClock.tama5.mon / 10) * 10 + (data >> 4);
                                    break;
                                case 0xa:
                                    gbClock.tama5.mon = (gbClock.tama5.mon % 10) + (data >> 4) * 10;
                                    break;
                                case 0xb:
                                    gbClock.tama5.y = (gbClock.tama5.y % 1000) + (data >> 4) * 1000;
                                    break;
                                case 0xc:
                                    gbClock.tama5.y = (gbClock.tama5.y % 100) + (gbClock.tama5.y / 1000) * 1000 + (data >> 4) * 100;
                                    break;
                                default:
                                    break;
                            }

                            writeClockStruct();
                        } else if(tama5RamByteSelect == 0x18) {
                            latchClock();

                            int seconds = (gbClock.tama5.s / 10) * 16 + gbClock.tama5.s % 10;
                            int secondsL = (gbClock.tama5.s % 10);
                            int secondsH = (gbClock.tama5.s / 10);
                            int minutes = (gbClock.tama5.m / 10) * 16 + gbClock.tama5.m % 10;
                            int hours = (gbClock.tama5.h / 10) * 16 + gbClock.tama5.h % 10;
                            int DaysL = gbClock.tama5.d % 10;
                            int DaysH = gbClock.tama5.d /10;
                            int MonthsL = gbClock.tama5.mon % 10;
                            int MonthsH = gbClock.tama5.mon / 10;
                            int Years3 = (gbClock.tama5.y / 100) % 10;
                            int Years4 = (gbClock.tama5.y / 1000);

                            switch(data & 0xF) {
                                case 0x0:
                                    tama5RAM[tama5RamByteSelect] = secondsL;
                                    break;
                                case 0x1:
                                    tama5RAM[tama5RamByteSelect] = secondsH;
                                    break;
                                case 0x7:
                                    tama5RAM[tama5RamByteSelect] = DaysL;
                                    break;
                                case 0x8:
                                    tama5RAM[tama5RamByteSelect] = DaysH;
                                    break;
                                case 0x9:
                                    tama5RAM[tama5RamByteSelect] = MonthsL;
                                    break;
                                case 0xA:
                                    tama5RAM[tama5RamByteSelect] = MonthsH;
                                    break;
                                case 0xB:
                                    tama5RAM[tama5RamByteSelect] = Years4;
                                    break;
                                case 0xC:
                                    tama5RAM[tama5RamByteSelect] = Years3;
                                    break;
                                default :
                                    break;
                            }

                            tama5RAM[0x54] = seconds;
                            tama5RAM[0x64] = minutes;
                            tama5RAM[0x74] = hours;
                            tama5RAM[0x84] = DaysH * 16 + DaysL;
                            tama5RAM[0x94] = MonthsH * 16 + MonthsL;

                            memory[0xA][0] = 1;
                        } else if(tama5RamByteSelect == 0x28) {
                            if((data & 0xF) == 0xB) {
                                gbClock.tama5.y = ((gbClock.tama5.y >> 2) << 2) + (data & 3);
                                writeClockStruct();
                            }
                        } else if(tama5RamByteSelect == 0x44) {
                            gbClock.tama5.m = (data / 16) * 10 + data % 16;
                            writeClockStruct();
                        } else if(tama5RamByteSelect == 0x54) {
                            gbClock.tama5.h = (data / 16) * 10 + data % 16;
                            writeClockStruct();
                        } else {
                            tama5RAM[tama5RamByteSelect] = data;
                        }
                    }
                }

                break;
            }
            case 1: {
                tama5CommandNumber = val;
                memory[0xA][1] = val;
                if(val == 0x0A) {
                    for(int i = 0; i < 0x10; i++) {
                        for(int j = 0; j < 0x10; j++) {
                            if(!(j & 2)) {
                                tama5RAM[((i * 0x10) + j) | 2] = tama5RAM[(i * 0x10) + j];
                            }
                        }
                    }

                    ramEnabled = true;
                    memory[0xA][0] = 1;
                } else if((val & 0x0E) == 0x0C) {
                    tama5RamByteSelect = tama5Commands[6] | (tama5Commands[7] << 4);

                    u8 byte = tama5RAM[tama5RamByteSelect];
                    memory[0xA][0] = (u8) ((val & 1) ? byte >> 4 : byte & 0x0F);

                    tama5Commands[0x0F] = 0;
                }

                break;
            }
            default:
                break;
        }
    } else if(ramEnabled && romFile->getRamBanks() > 0) {
        writeSram(addr & 0x1FFF, val);
    }
}


/* Increment y if x is greater than val */
#define OVERFLOW(x, val, y)   \
    do {                    \
        while (x >= val) {  \
            x -= val;       \
            y++;            \
        }                   \
    } while (0)

static int daysInMonth[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int daysInLeapMonth[12] = {
        31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

void Gameboy::latchClock() {
    time_t now;
    time(&now);

    time_t difference = now - gbClock.last;
    struct tm* lt = gmtime((const time_t*) &difference);

    switch(romFile->getMBC()) {
        case MBC3:
            gbClock.mbc3.s += lt->tm_sec;
            OVERFLOW(gbClock.mbc3.s, 60, gbClock.mbc3.m);
            gbClock.mbc3.m += lt->tm_min;
            OVERFLOW(gbClock.mbc3.m, 60, gbClock.mbc3.h);
            gbClock.mbc3.h += lt->tm_hour;
            OVERFLOW(gbClock.mbc3.h, 24, gbClock.mbc3.d);
            gbClock.mbc3.d += lt->tm_yday;
            /* Overflow! */
            if(gbClock.mbc3.d > 0x1FF) {
                /* Set the carry bit */
                gbClock.mbc3.ctrl |= 0x80;
                gbClock.mbc3.d &= 0x1FF;
            }

            /* The 9th bit of the day register is in the control register */
            gbClock.mbc3.ctrl &= ~1;
            gbClock.mbc3.ctrl |= (gbClock.mbc3.d > 0xff);
            break;
        case HUC3:
            gbClock.huc3.m += lt->tm_min;
            OVERFLOW(gbClock.huc3.m, 60 * 24, gbClock.huc3.d);
            gbClock.huc3.d += lt->tm_yday;
            OVERFLOW(gbClock.huc3.d, 365, gbClock.huc3.y);
            gbClock.huc3.y += lt->tm_year - 70;
            break;
        case TAMA5:
            gbClock.tama5.s += lt->tm_sec;
            OVERFLOW(gbClock.tama5.s, 60, gbClock.tama5.m);
            gbClock.tama5.m += lt->tm_min;
            OVERFLOW(gbClock.tama5.m, 60, gbClock.tama5.h);
            gbClock.tama5.h += lt->tm_hour;
            OVERFLOW(gbClock.tama5.h, 24, gbClock.tama5.d);
            gbClock.tama5.d += lt->tm_mday;
            OVERFLOW(gbClock.tama5.d, ((gbClock.tama5.y & 3) == 0 ? daysInLeapMonth : daysInMonth)[gbClock.tama5.mon], gbClock.tama5.mon);
            gbClock.tama5.mon += lt->tm_mon;
            OVERFLOW(gbClock.tama5.mon, 12, gbClock.tama5.y);
            gbClock.tama5.y += lt->tm_year - 70;
            break;
        default:
            break;
    }

    gbClock.last = (u32) now;
}
