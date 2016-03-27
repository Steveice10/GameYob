#include "platform/common/manager.h"
#include "platform/common/menu.h"
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

void Gameboy::powerOn(bool bios) {
    if(this->poweredOn) {
        return;
    }

    this->frontendEvents = 0;

    if(this->cartridge != NULL) {
        if((gbcModeOption == 1 && this->cartridge->isCgbRequired()) || (gbcModeOption == 2 && this->cartridge->isCgbSupported())) {
            if(sgbModeOption == 2 && this->cartridge->isSgbEnhanced()) {
                this->gbMode = MODE_SGB;
            } else {
                this->gbMode = MODE_CGB;
            }
        } else {
            if(sgbModeOption != 0 && this->cartridge->isSgbEnhanced()) {
                this->gbMode = MODE_SGB;
            } else {
                this->gbMode = MODE_GB;
            }
        }
    } else {
        this->gbMode = MODE_CGB;
    }

    this->biosOn = bios && ((biosMode == 1 && (gbBiosLoaded || gbcBiosLoaded)) || (biosMode == 2 && gbBiosLoaded) || (biosMode == 3 && gbcBiosLoaded));
    this->biosType = (biosMode == 1 && gbBiosLoaded && (!gbcBiosLoaded || this->gbMode != MODE_CGB)) || biosMode == 2 ? MODE_GB : MODE_CGB;

    if(this->biosOn) {
        this->gbMode = this->biosType;
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
    data.read((char*) &this->biosOn, sizeof(this->biosOn));
    data.read((char*) &this->biosType, sizeof(this->biosType));

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

    if((this->gbMode == MODE_GB && !gbBiosLoaded) || (this->gbMode == MODE_CGB && !gbcBiosLoaded)) {
        this->biosOn = false;
    }

    return true;
}

bool Gameboy::saveState(std::ostream& data) {
    data.write((char*) &STATE_VERSION, sizeof(STATE_VERSION));

    data.write((char*) &this->gbMode, sizeof(this->gbMode));
    data.write((char*) &this->biosOn, sizeof(this->biosOn));
    data.write((char*) &this->biosType, sizeof(this->biosType));

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

int Gameboy::run() {
    if(!this->poweredOn) {
        return RET_VBLANK;
    }

    while(this->frontendEvents == 0) {
        this->cpu->run();
    }

    int ret = this->frontendEvents;
    this->frontendEvents = 0;
    return ret;
}
