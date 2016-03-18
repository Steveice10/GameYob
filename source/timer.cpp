#include <stdio.h>

#include "platform/system.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "timer.h"

static const u8 timerShifts[] = {
        10, 4, 6, 8
};

Timer::Timer(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

void Timer::reset() {
    this->lastDividerCycle = 0;
    this->lastTimerCycle = 0;

    auto genericTimerRead = [this](u16 addr) -> u8 {
        this->updateTimer();
        return this->gameboy->mmu->readIO(addr);
    };

    auto genericTimerWrite = [this](u16 addr, u8 val) -> void {
        this->updateTimer();
        this->gameboy->mmu->writeIO(addr, val);
    };

    this->gameboy->mmu->mapIOReadFunc(DIV, [this](u16 addr) -> u8 {
        this->updateDivider();
        return this->gameboy->mmu->readIO(DIV);
    });

    this->gameboy->mmu->mapIOReadFunc(TIMA, genericTimerRead);
    this->gameboy->mmu->mapIOReadFunc(TMA, genericTimerRead);
    this->gameboy->mmu->mapIOReadFunc(TAC, genericTimerRead);

    this->gameboy->mmu->mapIOWriteFunc(DIV, [this](u16 addr, u8 val) -> void {
        this->updateDivider();
        this->gameboy->mmu->writeIO(DIV, 0);
    });

    this->gameboy->mmu->mapIOWriteFunc(TIMA, genericTimerWrite);
    this->gameboy->mmu->mapIOWriteFunc(TMA, genericTimerWrite);

    this->gameboy->mmu->mapIOWriteFunc(TAC, [this](u16 addr, u8 val) -> void {
        this->updateTimer();
        this->gameboy->mmu->writeIO(TAC, (u8) (val | 0xF8));

        u8 shift = timerShifts[val & 0x3];
        this->lastTimerCycle = this->gameboy->cpu->getCycle() >> shift << shift;
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
    u32 add = (u32) (this->gameboy->cpu->getCycle() - this->lastDividerCycle) >> 8;
    if(add > 0) {
        this->gameboy->mmu->writeIO(DIV, (u8) ((this->gameboy->mmu->readIO(DIV) + add) & 0xFF));
        this->lastDividerCycle += add << 8;
    }
}

void Timer::updateTimer() {
    u8 tac = this->gameboy->mmu->readIO(TAC);
    if((tac & 0x4) != 0) {
        u8 shift = timerShifts[tac & 0x3];

        u32 add = (u32) (this->gameboy->cpu->getCycle() - this->lastTimerCycle) >> shift;
        if(add > 0) {
            u32 counter = this->gameboy->mmu->readIO(TIMA) + add;
            if(counter >= 0x100) {
                u8 tma = this->gameboy->mmu->readIO(TMA);
                while(counter >= 0x100) {
                    counter = tma + (counter - 0x100);
                }

                this->gameboy->cpu->requestInterrupt(INT_TIMER);
            }

            this->gameboy->mmu->writeIO(TIMA, (u8) counter);
            this->lastTimerCycle += add << shift;
        }

        this->gameboy->cpu->setEventCycle(this->lastTimerCycle + ((0x100 - this->gameboy->mmu->readIO(TIMA)) << shift));
    }
}
