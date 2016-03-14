#include <stdio.h>

#include "platform/system.h"
#include "cpu.h"
#include "gameboy.h"
#include "printer.h"
#include "romfile.h"
#include "ir.h"
#include "mmu.h"

IR::IR(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

void IR::reset() {
    this->rp = 0;
}

void IR::loadState(FILE* file, int version) {
    fread(&this->rp, 1, sizeof(this->rp), file);
}

void IR::saveState(FILE* file) {
    fwrite(&this->rp, 1, sizeof(this->rp), file);
}

u8 IR::read(u16 addr) {
    switch(addr) {
        case RP:
            return this->rp | (u8) ((this->rp & 0x80) && systemGetIRState() ? 0 : (1 << 1));
        default:
            return 0;
    }
}

void IR::write(u16 addr, u8 val) {
    switch(addr) {
        case RP:
            systemSetIRState((val & (1 << 0)) == 1);
            this->rp = (u8) (val & ~(1 << 1));
            break;
        default:
            break;
    }
}