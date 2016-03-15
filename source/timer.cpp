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

    this->gameboy->mmu->mapIOReadFunc(DIV, [this](u16 addr, u8 val) -> u8 {
        this->updateDivider();
        return val;
    });

    this->gameboy->mmu->mapIOReadFunc(TIMA, [this](u16 addr, u8 val) -> u8 {
        this->updateTimer();
        return val;
    });

    this->gameboy->mmu->mapIOReadFunc(TMA, [this](u16 addr, u8 val) -> u8 {
        this->updateTimer();
        return val;
    });

    this->gameboy->mmu->mapIOReadFunc(TAC, [this](u16 addr, u8 val) -> u8 {
        this->updateTimer();
        return val;
    });

    this->gameboy->mmu->mapIOWriteFunc(DIV, [this](u16 addr, u8 val) -> u8 {
        this->updateDivider();
        return 0;
    });

    this->gameboy->mmu->mapIOWriteFunc(TIMA, [this](u16 addr, u8 val) -> u8 {
        this->updateTimer();
        return val;
    });

    this->gameboy->mmu->mapIOWriteFunc(TMA, [this](u16 addr, u8 val) -> u8 {
        this->updateTimer();
        return val;
    });

    this->gameboy->mmu->mapIOWriteFunc(TAC, [this](u16 addr, u8 val) -> u8 {
        this->updateTimer();
        this->lastTimerCycle = this->gameboy->cpu->getCycle() - (this->gameboy->cpu->getCycle() % timerPeriods[val & 0x3]);
        return val;
    });
}

void Timer::loadState(FILE* file, int version) {
    fread(&this->lastDividerCycle, 1, sizeof(this->lastDividerCycle), file);
    fread(&this->lastTimerCycle, 1, sizeof(this->lastTimerCycle), file);
}

void Timer::saveState(FILE* file) {
    fwrite(&this->lastDividerCycle, 1, sizeof(this->lastDividerCycle), file);
    fwrite(&this->lastTimerCycle, 1, sizeof(this->lastTimerCycle), file);
}

void Timer::update() {
    this->updateDivider();
    this->updateTimer();
}

void Timer::updateDivider() {
    if(this->gameboy->cpu->getCycle() > this->lastDividerCycle) {
        u32 add = (u32) (this->gameboy->cpu->getCycle() - this->lastDividerCycle) / 256;
        this->gameboy->mmu->writeIO(DIV, (u8) ((this->gameboy->mmu->readIO(DIV) + add) & 0xFF));
        this->lastDividerCycle += add * 256;
    }
}

void Timer::updateTimer() {
    u8 tac = this->gameboy->mmu->readIO(TAC);
    if((tac & 0x4) != 0) {
        u32 period = timerPeriods[tac & 0x3];

        if(this->gameboy->cpu->getCycle() >= this->lastTimerCycle + period) {
            u32 add = (u32) (this->gameboy->cpu->getCycle() - this->lastTimerCycle) / period;
            u32 counter = this->gameboy->mmu->readIO(TIMA) + add;
            if(counter >= 0x100) {
                u8 tma = this->gameboy->mmu->readIO(TMA);
                while(counter >= 0x100) {
                    counter = tma + (counter - 0x100);
                }

                this->gameboy->cpu->requestInterrupt(INT_TIMER);
            }

            this->gameboy->mmu->writeIO(TIMA, (u8) counter);
            this->lastTimerCycle += add * period;
        }

        this->gameboy->cpu->setEventCycle(this->lastTimerCycle + (period * (0x100 - this->gameboy->mmu->readIO(TIMA))));
    }
}
