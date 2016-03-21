#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "apu.h"
#include "cpu.h"
#include "gameboy.h"
#include "mbc.h"
#include "mmu.h"
#include "printer.h"
#include "ppu.h"
#include "sgb.h"
#include "romfile.h"
#include "serial.h"
#include "timer.h"

static const u8 STATE_VERSION = 11;

Gameboy::Gameboy() {
    this->romFile = NULL;

    this->mmu = new MMU(this);
    this->cpu = new CPU(this);
    this->ppu = new PPU(this);
    this->apu = new APU(this);
    this->mbc = new MBC(this);
    this->sgb = new SGB(this);
    this->timer = new Timer(this);
    this->serial = new Serial(this);
}

Gameboy::~Gameboy() {
    this->unloadRom();

    delete this->mmu;
    delete this->cpu;
    delete this->ppu;
    delete this->apu;
    delete this->mbc;
    delete this->sgb;
    delete this->timer;
    delete this->serial;
}

void Gameboy::reset(bool allowBios) {
    if(this->romFile == NULL) {
        return;
    }

    this->frontendEvents = 0;

    if((gbcModeOption == 1 && this->romFile->isCgbRequired()) || (gbcModeOption == 2 && this->romFile->isCgbSupported())) {
        if(sgbModeOption == 2 && this->romFile->isSgbEnhanced()) {
            this->gbMode = MODE_SGB;
        } else {
            this->gbMode = MODE_CGB;
        }
    } else {
        if(sgbModeOption != 0 && this->romFile->isSgbEnhanced()) {
            this->gbMode = MODE_SGB;
        } else {
            this->gbMode = MODE_GB;
        }
    }

    this->biosOn = allowBios && ((biosMode == 1 && ((this->gbMode != MODE_CGB && gbBiosLoaded) || gbcBiosLoaded)) || (biosMode == 2 && this->gbMode != MODE_CGB && gbBiosLoaded) || (biosMode == 3 && gbcBiosLoaded));
    if(this->biosOn) {
        if(biosMode == 1) {
            this->gbMode = this->gbMode != MODE_CGB && gbBiosLoaded ? MODE_GB : MODE_CGB;
        } else if(biosMode == 2) {
            this->gbMode = MODE_GB;
        } else if(biosMode == 3) {
            this->gbMode = MODE_CGB;
        }
    }

    this->mmu->reset();
    this->cpu->reset();
    this->ppu->reset();
    this->apu->reset();
    this->mbc->reset();
    this->sgb->reset();
    this->timer->reset();
    this->serial->reset();
}

bool Gameboy::loadState(std::istream& data) {
    if(!this->isRomLoaded()) {
        return false;
    }

    u8 version;
    data.read((char*) &version, sizeof(version));

    if(version < 11 || version > STATE_VERSION) {
        return false;
    }

    data.read((char*) &this->gbMode, sizeof(this->gbMode));
    data.read((char*) &this->biosOn, sizeof(this->biosOn));

    this->mmu->loadState(data, version);
    this->cpu->loadState(data, version);
    this->ppu->loadState(data, version);
    this->apu->loadState(data, version);
    this->mbc->loadState(data, version);
    this->sgb->loadState(data, version);
    this->timer->loadState(data, version);
    this->serial->loadState(data, version);

    if(this->biosOn && ((this->gbMode == MODE_GB && !gbBiosLoaded) || (this->gbMode == MODE_CGB && !gbcBiosLoaded))) {
        this->biosOn = false;
    }

    return true;
}

bool Gameboy::saveState(std::ostream& data) {
    if(!this->isRomLoaded()) {
        return false;
    }

    data.write((char*) &STATE_VERSION, sizeof(STATE_VERSION));
    data.write((char*) &this->gbMode, sizeof(this->gbMode));
    data.write((char*) &this->biosOn, sizeof(this->biosOn));

    this->mmu->saveState(data);
    this->cpu->saveState(data);
    this->ppu->saveState(data);
    this->apu->saveState(data);
    this->mbc->saveState(data);
    this->sgb->saveState(data);
    this->timer->saveState(data);
    this->serial->saveState(data);

    return true;
}

int Gameboy::run() {
    while(this->frontendEvents == 0) {
        this->cpu->run();
    }

    int ret = this->frontendEvents;
    this->frontendEvents = 0;
    return ret;
}

bool Gameboy::isRomLoaded() {
    return this->romFile != NULL;
}

bool Gameboy::loadRom(std::istream& data, int size) {
    this->unloadRom();

    this->romFile = new RomFile(data, size);
    return this->romFile != NULL;
}

void Gameboy::unloadRom() {
    if(this->romFile != NULL) {
        delete this->romFile;
        this->romFile = NULL;
    }
}
