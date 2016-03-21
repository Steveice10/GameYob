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

    this->printer->reset();

    this->gameboy->mmu->mapIOWriteFunc(SC, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(SC, val);

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
    });
}

void Serial::loadState(std::istream& data, u8 version) {
    data.read((char*) &this->nextSerialInternalCycle, sizeof(this->nextSerialInternalCycle));
    data.read((char*) &this->nextSerialExternalCycle, sizeof(this->nextSerialExternalCycle));

    this->printer->loadState(data, version);
}

void Serial::saveState(std::ostream& data) {
    data.write((char*) &this->nextSerialInternalCycle, sizeof(this->nextSerialInternalCycle));
    data.write((char*) &this->nextSerialExternalCycle, sizeof(this->nextSerialExternalCycle));

    this->printer->saveState(data);
}

void Serial::update() {
    // For external clock
    if(this->nextSerialExternalCycle > 0) {
        if(this->gameboy->cpu->getCycle() >= this->nextSerialExternalCycle) {
            u8 sc = this->gameboy->mmu->readIO(SC);

            this->nextSerialExternalCycle = 0;
            if((sc & 0x81) == 0x80) {
                this->gameboy->frontendEvents |= RET_LINK;
            }

            if(sc & 0x80) {
                this->gameboy->cpu->requestInterrupt(INT_SERIAL);
                this->gameboy->mmu->writeIO(SC, (u8) (sc & ~0x80));
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
                this->gameboy->mmu->writeIO(SB, this->printer->link(this->gameboy->mmu->readIO(SB)));
            } else {
                this->gameboy->mmu->writeIO(SB, 0xFF);
            }

            this->gameboy->cpu->requestInterrupt(INT_SERIAL);
        } else {
            this->gameboy->cpu->setEventCycle(this->nextSerialInternalCycle);
        }
    }

    if(printerEnabled) {
        this->printer->update();
    }
}