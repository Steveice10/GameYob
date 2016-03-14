#include <stdio.h>

#include "platform/system.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "timer.h"

static const u32 timerPeriods[] = {
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

    this->div = 0;
    this->tac = 0;
    this->tima = 0;
    this->tma = 0;
}

void Timer::loadState(FILE* file, int version) {
    fread(&this->lastDividerCycle, 1, sizeof(this->lastDividerCycle), file);
    fread(&this->lastTimerCycle, 1, sizeof(this->lastTimerCycle), file);
    fread(&this->div, 1, sizeof(this->div), file);
    fread(&this->tac, 1, sizeof(this->tac), file);
    fread(&this->tima, 1, sizeof(this->tima), file);
    fread(&this->tma, 1, sizeof(this->tma), file);
}

void Timer::saveState(FILE* file) {
    fwrite(&this->lastDividerCycle, 1, sizeof(this->lastDividerCycle), file);
    fwrite(&this->lastTimerCycle, 1, sizeof(this->lastTimerCycle), file);
    fwrite(&this->div, 1, sizeof(this->div), file);
    fwrite(&this->tac, 1, sizeof(this->tac), file);
    fwrite(&this->tima, 1, sizeof(this->tima), file);
    fwrite(&this->tma, 1, sizeof(this->tma), file);
}

void Timer::update() {
    this->updateDivider();
    this->updateTimer();
}

void Timer::updateDivider() {
    if(this->gameboy->cpu->getCycle() > this->lastDividerCycle) {
        u32 add = (u32) (this->gameboy->cpu->getCycle() - this->lastDividerCycle) / 256;
        this->div = (u8) ((this->div + add) & 0xFF);
        this->lastDividerCycle += add * 256;
    }
}

void Timer::updateTimer() {
    if((this->tac & 0x4) != 0) {
        u32 period = timerPeriods[this->tac & 0x3];

        if(this->gameboy->cpu->getCycle() >= this->lastTimerCycle + period) {
            u32 add = (u32) (this->gameboy->cpu->getCycle() - this->lastTimerCycle) / period;
            u32 counter = this->tima + add;
            if(counter >= 0x100) {
                while(counter >= 0x100) {
                    counter = this->tma + (counter - 0x100);
                }

                this->gameboy->cpu->requestInterrupt(INT_TIMER);
            }

            this->tima = (u8) counter;
            this->lastTimerCycle += add * period;
        }

        this->gameboy->cpu->setEventCycle(this->lastTimerCycle + (period * (0x100 - this->tima)));
    }
}

u8 Timer::read(u16 addr) {
    this->update();
    switch(addr) {
        case DIV:
            return this->div;
        case TIMA:
            return this->tima;
        case TMA:
            return this->tma;
        case TAC:
            return this->tac;
        default:
            return 0;
    }
}

void Timer::write(u16 addr, u8 val) {
    this->update();
    switch(addr) {
        case DIV:
            this->div = 0;
            break;
        case TIMA:
            this->tima = val;
            break;
        case TMA:
            this->tma = val;
            break;
        case TAC:
            this->tac = val;
            this->lastTimerCycle = this->gameboy->cpu->getCycle() - (this->gameboy->cpu->getCycle() % timerPeriods[this->tac & 0x3]);
            break;
        default:
            break;
    }
}
