#include <stdio.h>

#include "platform/common/menu.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "printer.h"
#include "romfile.h"
#include "serial.h"

// TODO: Needs work.

Serial::Serial(Gameboy* gameboy) {
    this->gameboy = gameboy;

    this->printer = new Printer(gameboy);
}

Serial::~Serial() {
    delete this->printer;
}

void Serial::reset() {
    this->nextSerialInternalCycle = 0;
    this->nextSerialExternalCycle = 0;

    this->sb = 0;
    this->sc = 0;

    this->printer->reset();
}

void Serial::loadState(FILE* file, int version) {
    fread(&this->nextSerialInternalCycle, 1, sizeof(this->nextSerialInternalCycle), file);
    fread(&this->nextSerialExternalCycle, 1, sizeof(this->nextSerialExternalCycle), file);
    fread(&this->sb, 1, sizeof(this->sb), file);
    fread(&this->sc, 1, sizeof(this->sc), file);

    this->printer->loadState(file, version);
}

void Serial::saveState(FILE* file) {
    fwrite(&this->nextSerialInternalCycle, 1, sizeof(this->nextSerialInternalCycle), file);
    fwrite(&this->nextSerialExternalCycle, 1, sizeof(this->nextSerialExternalCycle), file);
    fwrite(&this->sb, 1, sizeof(this->sb), file);
    fwrite(&this->sc, 1, sizeof(this->sc), file);

    this->printer->saveState(file);
}

int Serial::update() {
    int ret = 0;

    // For external clock
    if(this->nextSerialExternalCycle > 0) {
        if(this->gameboy->cpu->getCycle() >= this->nextSerialExternalCycle) {
            this->nextSerialExternalCycle = 0;
            if((this->sc & 0x81) == 0x80) {
                ret |= RET_LINK;
            }

            if(this->sc & 0x80) {
                this->gameboy->cpu->requestInterrupt(INT_SERIAL);
                this->sc &= ~0x80;
            }
        } else {
            this->gameboy->cpu->setEventCycle(this->nextSerialExternalCycle);
        }
    }

    // For internal clock
    if(this->nextSerialInternalCycle > 0) {
        if(this->gameboy->cpu->getCycle() >= this->nextSerialInternalCycle) {
            this->nextSerialInternalCycle = 0;
            if(printerEnabled && this->gameboy->romFile->getRomTitle().compare("ALLEY WAY") != 0) { // Alleyway breaks when the printer is enabled, so force disable it.
                this->sb = this->printer->link(this->sb);
            } else {
                this->sb = 0xFF;
            }

            this->gameboy->cpu->requestInterrupt(INT_SERIAL);
        } else {
            this->gameboy->cpu->setEventCycle(this->nextSerialInternalCycle);
        }
    }

    if(printerEnabled) {
        this->printer->update();
    }

    return ret;
}

u8 Serial::read(u16 addr) {
    switch(addr) {
        case SB:
            return this->sb;
        case SC:
            return this->sc;
        default:
            return 0;
    }
}

void Serial::write(u16 addr, u8 val) {
    switch(addr) {
        case SB:
            this->sb = val;
            break;
        case SC:
            this->sc = val;
            if((val & 0x81) == 0x81) { // Internal clock
                if(this->nextSerialInternalCycle == 0) {
                    if(this->gameboy->gbMode == MODE_CGB && (val & 0x02)) {
                        this->nextSerialInternalCycle = this->gameboy->cpu->getCycle() + CYCLES_PER_SECOND / (1024 * 32);
                    } else {
                        this->nextSerialInternalCycle = this->gameboy->cpu->getCycle() + CYCLES_PER_SECOND / 1024;
                    }

                    this->gameboy->cpu->setEventCycle(this->nextSerialInternalCycle);
                }
            } else {
                this->nextSerialInternalCycle = 0;
            }

            break;
        default:
            break;
    }
}