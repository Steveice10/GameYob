#include "apu.h"
#include "cartridge.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "printer.h"
#include "ppu.h"
#include "sgb.h"
#include "serial.h"
#include "timer.h"

static const u8 STATE_VERSION = 11;

Gameboy::Gameboy() {
    this->cartridge = NULL;

    this->mmu = new MMU(this);
    this->cpu = new CPU(this);
    this->ppu = new PPU(this);
    this->apu = new APU(this);
    this->sgb = new SGB(this);
    this->timer = new Timer(this);
    this->serial = new Serial(this);
}

Gameboy::~Gameboy() {
    this->cartridge = NULL;

    delete this->mmu;
    delete this->cpu;
    delete this->ppu;
    delete this->apu;
    delete this->sgb;
    delete this->timer;
    delete this->serial;
}

void Gameboy::powerOn() {
    if(this->poweredOn) {
        return;
    }

    if(this->cartridge != NULL) {
        if((this->settings.gbcModeOption == GBC_IF_NEEDED && this->cartridge->isCgbRequired()) || (this->settings.gbcModeOption == GBC_ON && this->cartridge->isCgbSupported())) {
            if(this->settings.sgbModeOption == SGB_PREFER_SGB && this->cartridge->isSgbEnhanced()) {
                this->gbMode = MODE_SGB;
            } else {
                this->gbMode = MODE_CGB;
            }
        } else {
            if(this->settings.sgbModeOption != SGB_OFF && this->cartridge->isSgbEnhanced()) {
                this->gbMode = MODE_SGB;
            } else {
                this->gbMode = MODE_GB;
            }
        }
    } else {
        this->gbMode = MODE_CGB;
    }

    this->mmu->reset();
    this->cpu->reset();
    this->ppu->reset();
    this->apu->reset();
    this->sgb->reset();
    this->timer->reset();
    this->serial->reset();

    if(this->cartridge != NULL) {
        this->cartridge->reset(this);
    }

    this->poweredOn = true;
}

void Gameboy::powerOff() {
    this->poweredOn = false;
}

bool Gameboy::loadState(std::istream& data) {
    u8 version;
    data.read((char*) &version, sizeof(version));

    if(version < 11 || version > STATE_VERSION) {
        return false;
    }

    data.read((char*) &this->gbMode, sizeof(this->gbMode));

    this->mmu->loadState(data, version);
    this->cpu->loadState(data, version);
    this->ppu->loadState(data, version);
    this->apu->loadState(data, version);
    this->sgb->loadState(data, version);
    this->timer->loadState(data, version);
    this->serial->loadState(data, version);

    if(this->cartridge != NULL) {
        this->cartridge->loadState(data, version);
    }

    return true;
}

bool Gameboy::saveState(std::ostream& data) {
    data.write((char*) &STATE_VERSION, sizeof(STATE_VERSION));

    data.write((char*) &this->gbMode, sizeof(this->gbMode));

    this->mmu->saveState(data);
    this->cpu->saveState(data);
    this->ppu->saveState(data);
    this->apu->saveState(data);
    this->sgb->saveState(data);
    this->timer->saveState(data);
    this->serial->saveState(data);

    if(this->cartridge != NULL) {
        this->cartridge->saveState(data);
    }

    return true;
}

void Gameboy::runFrame() {
    if(!this->poweredOn) {
        return;
    }

    this->ranFrame = false;
    this->audioSamplesWritten = 0;

    while(!this->ranFrame) {
        this->cpu->run();
    }
}
