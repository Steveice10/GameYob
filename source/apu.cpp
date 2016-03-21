#include "gb_apu/Gb_Apu.h"
#include "gb_apu/Multi_Buffer.h"

#include "platform/common/menu.h"
#include "platform/audio.h"
#include "apu.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"

APU::APU(Gameboy* gameboy) {
    this->gameboy = gameboy;

    this->buffer = new Stereo_Buffer();
    this->buffer->set_sample_rate((long) audioGetSampleRate());
    this->buffer->clock_rate(CYCLES_PER_SECOND);

    this->apu = new Gb_Apu();
    this->apu->set_output(this->buffer->center(), this->buffer->left(), this->buffer->right());
}

APU::~APU() {
    delete this->apu;
    delete this->buffer;
}

void APU::reset() {
    audioClear();

    this->lastSoundCycle = 0;
    this->halfSpeed = false;

    this->apu->reset(this->gameboy->gbMode == MODE_CGB ? Gb_Apu::mode_cgb : Gb_Apu::mode_dmg);
    this->buffer->clear();

    for(u16 regAddr = NR10; regAddr <= WAVEF; regAddr++) {
        this->gameboy->mmu->mapIOReadFunc(regAddr, [this](u16 addr) -> u8 {
            return (u8) this->apu->read_register((u32) (this->gameboy->cpu->getCycle() - this->lastSoundCycle) >> this->halfSpeed, addr);
        });

        this->gameboy->mmu->mapIOWriteFunc(regAddr, [this](u16 addr, u8 val) -> void {
            this->apu->write_register((u32) (this->gameboy->cpu->getCycle() - this->lastSoundCycle) >> this->halfSpeed, addr, val);
        });
    }
}

void APU::loadState(std::istream& data, u8 version) {
    gb_apu_state_t apuState;
    data.read((char*) &apuState, sizeof(apuState));
    data.read((char*) &this->lastSoundCycle, sizeof(this->lastSoundCycle));
    data.read((char*) &this->halfSpeed, sizeof(this->halfSpeed));

    this->apu->load_state(apuState);
    audioClear();
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

        if(soundEnabled) {
            long available = this->buffer->samples_avail();
            u32* buf = new u32[available / 2 * sizeof(u32)];

            long count = this->buffer->read_samples((s16*) buf, available);
            audioPlay(buf, count / 2);

            delete buf;
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
        this->lastSoundCycle += (this->gameboy->cpu->getCycle() - this->lastSoundCycle) / 2;
    }

    this->halfSpeed = halfSpeed;
}

void APU::setChannelEnabled(int channel, bool enabled) {
    this->apu->set_osc_enabled(channel, enabled);
}

