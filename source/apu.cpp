#include "gb_apu/Gb_Apu.h"
#include "gb_apu/Multi_Buffer.h"

#include "apu.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"

APU::APU(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

APU::~APU() {
    if(this->buffer != NULL) {
        delete this->buffer;
        this->buffer = NULL;
    }

    if(this->apu != NULL) {
        delete this->apu;
        this->apu = NULL;
    }
}

void APU::reset() {
    if(this->buffer != NULL) {
        delete this->buffer;
        this->buffer = NULL;
    }

    if(this->apu != NULL) {
        delete this->apu;
        this->apu = NULL;
    }

    this->buffer = new Stereo_Buffer();
    this->buffer->set_sample_rate((long) this->gameboy->settings.audioSampleRate);
    this->buffer->clock_rate(CYCLES_PER_SECOND);
    this->buffer->clear();

    this->apu = new Gb_Apu(gameboy);
    this->apu->set_output(this->buffer->center(), this->buffer->left(), this->buffer->right());
    this->apu->reset();

    this->lastSoundCycle = 0;
    this->halfSpeed = false;

    auto read = [this](u16 addr) -> u8 {
        return (u8) this->apu->read_register((u32) (this->gameboy->cpu->getCycle() - this->lastSoundCycle) >> this->halfSpeed, addr);
    };

    auto write = [this](u16 addr, u8 val) -> void {
        this->apu->write_register((u32) (this->gameboy->cpu->getCycle() - this->lastSoundCycle) >> this->halfSpeed, addr, val);
    };

    for(u16 regAddr = NR10; regAddr <= WAVEF; regAddr++) {
        this->gameboy->mmu->mapIOReadFunc(regAddr, read);
        this->gameboy->mmu->mapIOWriteFunc(regAddr, write);
    }

    for(u16 regAddr = PCM12; regAddr <= PCM34; regAddr++) {
        this->gameboy->mmu->mapIOReadFunc(regAddr, read);
        this->gameboy->mmu->mapIOWriteFunc(regAddr, write);
    }
}

void APU::loadState(std::istream& data, u8 version) {
    gb_apu_state_t apuState;
    data.read((char*) &apuState, sizeof(apuState));
    data.read((char*) &this->lastSoundCycle, sizeof(this->lastSoundCycle));
    data.read((char*) &this->halfSpeed, sizeof(this->halfSpeed));

    this->apu->load_state(apuState);
}

void APU::saveState(std::ostream& data) {
    gb_apu_state_t apuState;
    this->apu->save_state(&apuState);

    data.write((char*) &apuState, sizeof(apuState));
    data.write((char*) &this->lastSoundCycle, sizeof(this->lastSoundCycle));
    data.write((char*) &this->halfSpeed, sizeof(this->halfSpeed));
}

void APU::update() {
    if(this->gameboy->cpu->getCycle() >= this->lastSoundCycle + (CYCLES_PER_FRAME << this->halfSpeed)) {
        u32 cycles = (u32) (this->gameboy->cpu->getCycle() - this->lastSoundCycle) >> this->halfSpeed;

        this->apu->end_frame(cycles);
        this->buffer->end_frame(cycles);

        this->lastSoundCycle = this->gameboy->cpu->getCycle();

        if(this->gameboy->settings.soundEnabled && this->gameboy->settings.audioBuffer != NULL) {
            long space = this->gameboy->settings.audioSamples - this->gameboy->audioSamplesWritten;
            long read = this->buffer->samples_avail() / 2;
            if(read > space) {
                read = space;
            }

            this->gameboy->audioSamplesWritten += this->buffer->read_samples((s16*) &this->gameboy->settings.audioBuffer[this->gameboy->audioSamplesWritten], read * 2) / 2;
        } else {
            this->buffer->clear();
        }
    }

    this->gameboy->cpu->setEventCycle(this->lastSoundCycle + (CYCLES_PER_FRAME << this->halfSpeed));
}

void APU::setHalfSpeed(bool halfSpeed) {
    if(!this->halfSpeed && halfSpeed) {
        this->lastSoundCycle -= this->gameboy->cpu->getCycle() - this->lastSoundCycle;
    } else if(this->halfSpeed && !halfSpeed) {
        this->lastSoundCycle += (this->gameboy->cpu->getCycle() - this->lastSoundCycle) >> 1;
    }

    this->halfSpeed = halfSpeed;
}

