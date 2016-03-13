#include <sys/stat.h>
#include <stdio.h>

#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "apu.h"
#include "cpu.h"
#include "gameboy.h"
#include "ir.h"
#include "mbc.h"
#include "mmu.h"
#include "printer.h"
#include "ppu.h"
#include "sgb.h"
#include "romfile.h"
#include "serial.h"
#include "timer.h"

static const int STATE_VERSION = 11;

Gameboy::Gameboy() {
    this->romFile = NULL;

    this->cpu = new CPU(this);
    this->timer = new Timer(this);
    this->mmu = new MMU(this);
    this->ppu = new PPU(this);
    this->apu = new APU(this);
    this->sgb = new SGB(this);
    this->mbc = new MBC(this);
    this->serial = new Serial(this);
    this->ir = new IR(this);
}

Gameboy::~Gameboy() {
    this->unloadRom();

    delete this->cpu;
    delete this->timer;
    delete this->mbc;
    delete this->mmu;
    delete this->ppu;
    delete this->apu;
    delete this->sgb;
    delete this->serial;
    delete this->ir;
}

void Gameboy::reset(bool allowBios) {
    if(this->romFile == NULL) {
        return;
    }

    this->pickMode();

    this->biosOn = allowBios && !this->romFile->isGBS() && ((biosMode == 1 && ((this->gbMode != MODE_CGB && gbBiosLoaded) || gbcBiosLoaded)) || (biosMode == 2 && this->gbMode != MODE_CGB && gbBiosLoaded) || (biosMode == 3 && gbcBiosLoaded));
    if(this->biosOn) {
        if(biosMode == 1) {
            this->gbMode = this->gbMode != MODE_CGB && gbBiosLoaded ? MODE_GB : MODE_CGB;
        } else if(biosMode == 2) {
            this->gbMode = MODE_GB;
        } else if(biosMode == 3) {
            this->gbMode = MODE_CGB;
        }
    }

    this->cpu->reset();
    this->timer->reset();
    this->mmu->reset();
    this->ppu->reset();
    this->apu->reset();
    this->sgb->reset();
    this->mbc->reset();
    this->serial->reset();
    this->ir->reset();
}

bool Gameboy::loadState(FILE* file) {
    if(!isRomLoaded() || file == NULL) {
        return false;
    }

    int version;
    fread(&version, 1, sizeof(version), file);

    if(version < 11 || version > STATE_VERSION) {
        return false;
    }

    fread(&this->gbMode, 1, sizeof(this->gbMode), file);
    fread(&this->biosOn, 1, sizeof(this->biosOn), file);

    this->cpu->loadState(file, version);
    this->timer->loadState(file, version);
    this->mmu->loadState(file, version);
    this->ppu->loadState(file, version);
    this->apu->loadState(file, version);
    this->sgb->loadState(file, version);
    this->mbc->loadState(file, version);
    this->serial->loadState(file, version);
    this->ir->loadState(file, version);

    if(this->biosOn && ((this->gbMode == MODE_GB && !gbBiosLoaded) || (this->gbMode == MODE_CGB && !gbcBiosLoaded))) {
        this->biosOn = false;
    }

    return true;
}

bool Gameboy::saveState(FILE* file) {
    if(!isRomLoaded() || file == NULL) {
        return false;
    }

    fwrite(&STATE_VERSION, 1, sizeof(int), file);

    fwrite(&this->gbMode, 1, sizeof(this->gbMode), file);
    fwrite(&this->biosOn, 1, sizeof(this->biosOn), file);

    this->cpu->saveState(file);
    this->timer->saveState(file);
    this->mmu->saveState(file);
    this->ppu->saveState(file);
    this->apu->saveState(file);
    this->sgb->saveState(file);
    this->mbc->saveState(file);
    this->serial->saveState(file);
    this->ir->saveState(file);

    return true;
}

int Gameboy::run() {
    return this->cpu->run(Gameboy::pollEvents);
}

void Gameboy::checkInput() {
    u8 p1 = this->mmu->read(0xFF00);
    if(!(p1 & 0x10)) {
        if((this->sgb->getController(0) & 0xf0) != 0xf0) {
            this->cpu->requestInterrupt(INT_JOYPAD);
        }
    } else if(!(p1 & 0x20)) {
        if((this->sgb->getController(0) & 0x0f) != 0x0f) {
            this->cpu->requestInterrupt(INT_JOYPAD);
        }
    }
}

void Gameboy::pickMode() {
    if(this->romFile->isGBS()) {
        this->gbMode = MODE_CGB;
    } else {
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
    }
}

int Gameboy::pollEvents(Gameboy* gameboy) {
    int ret = 0;
    gameboy->timer->update();
    gameboy->apu->update();
    ret |= gameboy->serial->update();
    ret |= gameboy->ppu->update();
    return ret;
}

bool Gameboy::isRomLoaded() {
    return this->romFile != NULL;
}

bool Gameboy::loadRomFile(const char* filename) {
    if(this->romFile != NULL) {
        delete this->romFile;
        this->romFile = NULL;
    }

    if(filename == NULL) {
        return true;
    }

    this->romFile = new RomFile(this, filename);
    if(this->romFile == NULL) {
        return false;
    }

    if(!this->romFile->isLoaded()) {
        delete this->romFile;
        this->romFile = NULL;
        return false;
    }

    return true;
}

void Gameboy::unloadRom() {
    if(this->romFile != NULL) {
        delete this->romFile;
        this->romFile = NULL;
    }
}
