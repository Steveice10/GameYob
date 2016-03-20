#include <sys/stat.h>
#include <stdio.h>

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

static const int STATE_VERSION = 11;

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

    this->mmu->loadState(file, version);
    this->cpu->loadState(file, version);
    this->ppu->loadState(file, version);
    this->apu->loadState(file, version);
    this->mbc->loadState(file, version);
    this->sgb->loadState(file, version);
    this->timer->loadState(file, version);
    this->serial->loadState(file, version);

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

    this->mmu->saveState(file);
    this->cpu->saveState(file);
    this->ppu->saveState(file);
    this->apu->saveState(file);
    this->mbc->saveState(file);
    this->sgb->saveState(file);
    this->timer->saveState(file);
    this->serial->saveState(file);

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

bool Gameboy::loadRom(u8* rom, u32 size) {
    this->unloadRom();

    if(rom == NULL) {
        return true;
    }

    this->romFile = new RomFile(rom, size);
    return this->romFile != NULL;
}

void Gameboy::unloadRom() {
    if(this->romFile != NULL) {
        delete this->romFile;
        this->romFile = NULL;
    }
}
