#include <stdio.h>

#include "platform/system.h"
#include "cpu.h"
#include "gameboy.h"
#include "timer.h"

static const int timerPeriods[] = {
        CYCLES_PER_SECOND / 4096,
        CYCLES_PER_SECOND / 262144,
        CYCLES_PER_SECOND / 65536,
        CYCLES_PER_SECOND / 16384,
};

Timer::Timer(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

void Timer::reset() {
    this->lastDividerCycle = 0;
    this->lastTimerCycle = 0;
    this->timerCatchUpCycles = 0;

    this->dividerCounter = 0;
    this->timerEnabled = false;
    this->timerPeriod = 0;
    this->timerCounter = 0;
    this->timerModulo = 0;
}

void Timer::loadState(FILE* file, int version) {
    fread(&this->lastDividerCycle, 1, sizeof(this->lastDividerCycle), file);
    fread(&this->lastTimerCycle, 1, sizeof(this->lastTimerCycle), file);
    fread(&this->timerCatchUpCycles, 1, sizeof(this->timerCatchUpCycles), file);
    fread(&this->dividerCounter, 1, sizeof(this->dividerCounter), file);
    fread(&this->timerEnabled, 1, sizeof(this->timerEnabled), file);
    fread(&this->timerPeriod, 1, sizeof(this->timerPeriod), file);
    fread(&this->timerCounter, 1, sizeof(this->timerCounter), file);
    fread(&this->timerModulo, 1, sizeof(this->timerModulo), file);
}

void Timer::saveState(FILE* file) {
    fwrite(&this->lastDividerCycle, 1, sizeof(this->lastDividerCycle), file);
    fwrite(&this->lastTimerCycle, 1, sizeof(this->lastTimerCycle), file);
    fwrite(&this->timerCatchUpCycles, 1, sizeof(this->timerCatchUpCycles), file);
    fwrite(&this->dividerCounter, 1, sizeof(this->dividerCounter), file);
    fwrite(&this->timerEnabled, 1, sizeof(this->timerEnabled), file);
    fwrite(&this->timerPeriod, 1, sizeof(this->timerPeriod), file);
    fwrite(&this->timerCounter, 1, sizeof(this->timerCounter), file);
    fwrite(&this->timerModulo, 1, sizeof(this->timerModulo), file);
}

void Timer::update() {
    this->updateDivider();
    this->updateTimer();
}

void Timer::updateDivider() {
    if(this->gameboy->cpu->getCycle() > this->lastDividerCycle) {
        u32 add = (u32) (this->gameboy->cpu->getCycle() - this->lastDividerCycle) / 256;
        this->dividerCounter = (u8) ((this->dividerCounter + add) & 0xFF);
        this->lastDividerCycle += add * 256;
    }
}

void Timer::updateTimer() {
    if(this->timerEnabled) {
        int period = timerPeriods[this->timerPeriod];

        if(this->gameboy->cpu->getCycle() >= this->lastTimerCycle + period) {
            u32 add = (u32) (this->gameboy->cpu->getCycle() - this->lastTimerCycle) / period;
            u32 counter = this->timerCounter + add;
            if(counter >= 0x100) {
                while(counter >= 0x100) {
                    counter = this->timerModulo + (counter - 0x100);
                }

                this->gameboy->cpu->requestInterrupt(INT_TIMER);
            }

            this->timerCounter = (u8) counter;
            this->lastTimerCycle += add * period;
        }

        this->gameboy->cpu->setEventCycle(this->lastTimerCycle + (period * (0x100 - this->timerCounter)));
    }
}

u8 Timer::read(u16 addr) {
    switch(addr) {
        case 0xFF04:
            this->updateDivider();
            return this->dividerCounter;
        case 0xFF05:
            this->updateTimer();
            return this->timerCounter;
        case 0xFF06:
            return this->timerModulo;
        case 0xFF07:
            return (u8) ((this->timerPeriod & 0x3) | ((this->timerEnabled & 0x1) << 2));
        default:
            return 0;
    }
}

void Timer::write(u16 addr, u8 val) {
    switch(addr) {
        case 0xFF04:
            this->updateDivider();
            this->dividerCounter = 0;
            break;
        case 0xFF05:
            this->updateTimer();
            this->timerCounter = val;
            break;
        case 0xFF06:
            this->timerModulo = val;
            break;
        case 0xFF07: {
            this->updateTimer();

            bool wasEnabled = this->timerEnabled;

            this->timerPeriod = (u8) (val & 0x3);
            this->timerEnabled = (bool) ((val >> 2) & 0x1);

            if(!this->timerEnabled && wasEnabled) {
                this->timerCatchUpCycles = (u32) (this->gameboy->cpu->getCycle() - this->lastTimerCycle);
            } else if(this->timerEnabled && !wasEnabled) {
                this->lastTimerCycle = this->gameboy->cpu->getCycle() + this->timerCatchUpCycles;
                this->timerCatchUpCycles = 0;
            }

            this->updateTimer();
            break;
        }
        default:
            break;
    }
}
