#include <string.h>

#include "platform/common/menu.h"
#include "platform/system.h"
#include "types.h"
#include "apu.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "ppu.h"

CPU::CPU(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

void CPU::reset() {
    this->cycleCount = 0;
    this->eventCycle = 0;
    memset(&this->registers, 0, sizeof(this->registers));
    this->haltState = false;
    this->haltBug = false;
    this->ime = false;

    if(this->gameboy->biosOn) {
        this->registers.pc.w = 0;
    } else {
        this->registers.pc.w = 0x100;
    }

    this->registers.af.b.l = 0xB0;
    this->registers.bc.w = 0x0013;
    this->registers.de.w = 0x00D8;
    this->registers.hl.w = 0x014D;
    if(this->gameboy->gbMode == MODE_CGB) {
        this->registers.af.b.h = 0x11;
        if(gbaModeOption) {
            this->registers.bc.b.h |= 1;
        }
    } else {
        this->registers.af.b.h = 0x01;
    }

    this->gameboy->mmu->mapIOWriteFunc(KEY1, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(KEY1, (u8) ((this->gameboy->mmu->readIO(KEY1) & 0x80) | (val & 1)));
    });
}

void CPU::loadState(FILE* file, int version) {
    fread(&this->cycleCount, 1, sizeof(this->cycleCount), file);
    fread(&this->eventCycle, 1, sizeof(this->eventCycle), file);
    fread(&this->registers, 1, sizeof(this->registers), file);
    fread(&this->haltState, 1, sizeof(this->haltState), file);
    fread(&this->haltBug, 1, sizeof(this->haltBug), file);
    fread(&this->ime, 1, sizeof(this->ime), file);
}

void CPU::saveState(FILE* file) {
    fwrite(&this->cycleCount, 1, sizeof(this->cycleCount), file);
    fwrite(&this->eventCycle, 1, sizeof(this->eventCycle), file);
    fwrite(&this->registers, 1, sizeof(this->registers), file);
    fwrite(&this->haltState, 1, sizeof(this->haltState), file);
    fwrite(&this->haltBug, 1, sizeof(this->haltBug), file);
    fwrite(&this->ime, 1, sizeof(this->ime), file);
}

int CPU::run(int (*pollEvents)(Gameboy* gameboy)) {
    int ret = 0;
    while(ret == 0) {
        if(!this->haltState) {
            while(this->cycleCount < this->eventCycle) {
                this->runInstruction();
            }
        } else {
            this->cycleCount = this->eventCycle;
        }

        this->eventCycle = UINT64_MAX;

        int prevTriggered = this->gameboy->mmu->readIO(IF) & this->gameboy->mmu->readIO(IE);
        ret = pollEvents(this->gameboy);
        int triggered = this->gameboy->mmu->readIO(IF) & this->gameboy->mmu->readIO(IE);

        if(triggered != 0) {
            /* Hack to fix Robocop 2 and LEGO Racers, possibly others.
             * Interrupts can occur in the middle of an opcode. The result of
             * this is that said opcode can read the resulting state - most
             * importantly, it can read LY=144 before the vblank interrupt takes
             * over. This is a decent approximation of that effect.
             * This has been known to break Megaman V boss intros, that's fixed
             * by the "prevTriggered" stuff.
             */
            if(!this->haltState && prevTriggered != triggered) {
                this->runInstruction();
                triggered = this->gameboy->mmu->readIO(IF) & this->gameboy->mmu->readIO(IE);
            }

            if(triggered != 0) {
                this->haltState = false;
                if(this->ime) {
                    this->ime = false;

                    this->gameboy->mmu->write(--this->registers.sp.w, this->registers.pc.b.h);
                    this->gameboy->mmu->write(--this->registers.sp.w, this->registers.pc.b.l);

                    int irqNo = __builtin_ffs(triggered) - 1;
                    this->registers.pc.w = (u16) (0x40 + (irqNo << 3));
                    this->gameboy->mmu->writeIO(IF, (u8) (this->gameboy->mmu->readIO(IF) & ~(1 << irqNo)));

                    this->advanceCycles(20);
                }
            }
        }
    }

    return ret;
}

void CPU::setDoubleSpeed(bool doubleSpeed) {
    if(doubleSpeed) {
        this->gameboy->mmu->writeIO(KEY1, (u8) (this->gameboy->mmu->readIO(KEY1) | 0x80));
    } else {
        this->gameboy->mmu->writeIO(KEY1, (u8) (this->gameboy->mmu->readIO(KEY1) & ~0x80));
    }

    this->gameboy->apu->setHalfSpeed(doubleSpeed);
    this->gameboy->ppu->setHalfSpeed(doubleSpeed);
}

void CPU::requestInterrupt(int id) {
    this->gameboy->mmu->writeIO(IF, (u8) (this->gameboy->mmu->readIO(IF) | id));
}

void CPU::runInstruction() {
    u8 op = this->gameboy->mmu->read(this->registers.pc.w);

    if(!this->haltBug) {
        this->registers.pc.w++;
    } else {
        this->haltBug = false;
    }

    (this->*opcodes[op])();

    if(this->haltState || (this->ime && (this->gameboy->mmu->readIO(IF) & this->gameboy->mmu->readIO(IE)) != 0)) {
        this->setEventCycle(this->cycleCount);
    }
}

#define FLAG_ZERO 0x80
#define FLAG_NEGATIVE 0x40
#define FLAG_HALFCARRY 0x20
#define FLAG_CARRY 0x10

#define FLAG_ISZERO ((this->registers.af.b.l & FLAG_ZERO) == FLAG_ZERO)
#define FLAG_ISNEGATIVE ((this->registers.af.b.l & FLAG_NEGATIVE) == FLAG_NEGATIVE)
#define FLAG_ISCARRY ((this->registers.af.b.l & FLAG_CARRY) == FLAG_CARRY)
#define FLAG_ISHALFCARRY ((this->registers.af.b.l & FLAG_HALFCARRY) == FLAG_HALFCARRY)

#define FLAG_SET(f, x) (this->registers.af.b.l ^= (-(x) ^ this->registers.af.b.l) & (f))

#define READPC8() ({                                         \
    u8 b = this->gameboy->mmu->read(this->registers.pc.w++); \
    b;                                                       \
})

#define READPC16() (READPC8() | (READPC8() << 8))

#define PUSH(d)                                                           \
    this->gameboy->mmu->write(--this->registers.sp.w, ((d) >> 8) & 0xFF); \
    this->gameboy->mmu->write(--this->registers.sp.w, (d) & 0xFF);

#define POP(d)                                                \
    u8 lo = this->gameboy->mmu->read(this->registers.sp.w++); \
    u8 hi = this->gameboy->mmu->read(this->registers.sp.w++); \
    (d) = lo | (hi << 8);

void CPU::undefined() {
    systemPrintDebug("Undefined instruction 0x%x at 0x%x\n", this->gameboy->mmu->read((u16) (this->registers.pc.w - 1)), this->registers.pc.w - 1);
}

void CPU::nop() {
    this->advanceCycles(4);
}

void CPU::ld_bc_nn() {
    this->registers.bc.w = READPC16();
    this->advanceCycles(12);
}

void CPU::ld_bcp_a() {
    this->gameboy->mmu->write(this->registers.bc.w, this->registers.af.b.h);
    this->advanceCycles(8);
}

void CPU::inc_bc() {
    this->registers.bc.w++;
    this->advanceCycles(8);
}

void CPU::inc_b() {
    u8 result = (u8) (this->registers.bc.b.h + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.bc.b.h & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
    this->advanceCycles(4);
}

void CPU::dec_b() {
    u8 result = (u8) (this->registers.bc.b.h - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.bc.b.h & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
    this->advanceCycles(4);
}

void CPU::ld_b_n() {
    this->registers.bc.b.h = READPC8();
    this->advanceCycles(8);
}

void CPU::rlca() {
    u8 carry = (u8) ((this->registers.af.b.h & 0x80) >> 7);
    u8 result = (this->registers.af.b.h << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::ld_nnp_sp() {
    u16 addr = READPC16();
    this->gameboy->mmu->write(addr, (u8) (this->registers.sp.w & 0xFF));
    this->gameboy->mmu->write((u16) (addr + 1), (u8) ((this->registers.sp.w >> 8) & 0xFF));
    this->advanceCycles(20);
}

void CPU::add_hl_bc() {
    int result = this->registers.hl.w + this->registers.bc.w;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.w & 0xFFF) + (this->registers.bc.w & 0xFFF) >= 0x1000);
    FLAG_SET(FLAG_CARRY, result >= 0x10000);
    this->registers.hl.w = (u16) (result & 0xFFFF);
    this->advanceCycles(8);
}

void CPU::ld_a_bcp() {
    this->registers.af.b.h = this->gameboy->mmu->read(this->registers.bc.w);
    this->advanceCycles(8);
}

void CPU::dec_bc() {
    this->registers.bc.w--;
    this->advanceCycles(8);
}

void CPU::inc_c() {
    u8 result = (u8) (this->registers.bc.b.l + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.bc.b.l & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
    this->advanceCycles(4);
}

void CPU::dec_c() {
    u8 result = (u8) (this->registers.bc.b.l - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.bc.b.l & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
    this->advanceCycles(4);
}

void CPU::ld_c_n() {
    this->registers.bc.b.l = READPC8();
    this->advanceCycles(8);
}

void CPU::rrca() {
    u8 carry = (u8) ((this->registers.af.b.h & 0x01) << 7);
    u8 result = (this->registers.af.b.h >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::stop() {
    u8 key1 = this->gameboy->mmu->readIO(KEY1);
    if((key1 & 1) && this->gameboy->gbMode == MODE_CGB) {
        this->setDoubleSpeed(!(key1 & 0x80));

        this->gameboy->mmu->writeIO(KEY1, (u8) (this->gameboy->mmu->readIO(KEY1) & ~1));
        this->registers.pc.w++;
    } else {
        this->haltState = true;
    }

    this->advanceCycles(4);
}

void CPU::ld_de_nn() {
    this->registers.de.w = READPC16();
    this->advanceCycles(12);
}

void CPU::ld_dep_a() {
    this->gameboy->mmu->write(this->registers.de.w, this->registers.af.b.h);
    this->advanceCycles(8);
}

void CPU::inc_de() {
    this->registers.de.w++;
    this->advanceCycles(8);
}

void CPU::inc_d() {
    u8 result = (u8) (this->registers.de.b.h + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.de.b.h & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
    this->advanceCycles(4);
}

void CPU::dec_d() {
    u8 result = (u8) (this->registers.de.b.h - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.de.b.h & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
    this->advanceCycles(4);
}

void CPU::ld_d_n() {
    this->registers.de.b.h = READPC8();
    this->advanceCycles(8);
}

void CPU::rla() {
    u8 result = (this->registers.af.b.h << 1) | (u8) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::jr_n() {
    this->registers.pc.w += (s8) READPC8();
    this->advanceCycles(12);
}

void CPU::add_hl_de() {
    int result = this->registers.hl.w + this->registers.de.w;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.w & 0xFFF) + (this->registers.de.w & 0xFFF) >= 0x1000);
    FLAG_SET(FLAG_CARRY, result >= 0x10000);
    this->registers.hl.w = (u16) (result & 0xFFFF);
    this->advanceCycles(8);
}

void CPU::ld_a_dep() {
    this->registers.af.b.h = this->gameboy->mmu->read(this->registers.de.w);
    this->advanceCycles(8);
}

void CPU::dec_de() {
    this->registers.de.w--;
    this->advanceCycles(8);
}

void CPU::inc_e() {
    u8 result = (u8) (this->registers.de.b.l + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.de.b.l & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
    this->advanceCycles(4);
}

void CPU::dec_e() {
    u8 result = (u8) (this->registers.de.b.l - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.de.b.l & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
    this->advanceCycles(4);
}

void CPU::ld_e_n() {
    this->registers.de.b.l = READPC8();
    this->advanceCycles(8);
}

void CPU::rra() {
    u8 result = (this->registers.af.b.h >> 1) | (u8) (FLAG_ISCARRY << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::jr_nz_n() {
    s8 offset = (s8) READPC8();
    if(!FLAG_ISZERO) {
        this->registers.pc.w += offset;
        this->advanceCycles(12);
    } else {
        this->advanceCycles(8);
    }
}

void CPU::ld_hl_nn() {
    this->registers.hl.w = READPC16();
    this->advanceCycles(12);
}

void CPU::ldi_hlp_a() {
    this->gameboy->mmu->write(this->registers.hl.w++, this->registers.af.b.h);
    this->advanceCycles(8);
}

void CPU::inc_hl() {
    this->registers.hl.w++;
    this->advanceCycles(8);
}

void CPU::inc_h() {
    u8 result = (u8) (this->registers.hl.b.h + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.b.h & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
    this->advanceCycles(4);
}

void CPU::dec_h() {
    u8 result = (u8) (this->registers.hl.b.h - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.b.h & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
    this->advanceCycles(4);
}

void CPU::ld_h_n() {
    this->registers.hl.b.h = READPC8();
    this->advanceCycles(8);
}

void CPU::daa() {
    int result = this->registers.af.b.h;
    if(FLAG_ISNEGATIVE) {
        if(FLAG_ISHALFCARRY) {
            result += 0xFA;
        }

        if(FLAG_ISCARRY) {
            result += 0xA0;
        }
    } else {
        if(FLAG_ISHALFCARRY || (result & 0xF) > 9) {
            result += 0x06;
        }

        if(FLAG_ISCARRY || (result & 0x1F0) > 0x90) {
            result += 0x60;
            FLAG_SET(FLAG_CARRY, 1);
        } else {
            FLAG_SET(FLAG_CARRY, 0);
        }
    }

    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::jr_z_n() {
    s8 offset = (s8) READPC8();
    if(FLAG_ISZERO) {
        this->registers.pc.w += offset;
        this->advanceCycles(12);
    } else {
        this->advanceCycles(8);
    }
}

void CPU::add_hl_hl() {
    int result = this->registers.hl.w + this->registers.hl.w;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.w & 0xFFF) + (this->registers.hl.w & 0xFFF) >= 0x1000);
    FLAG_SET(FLAG_CARRY, result >= 0x10000);
    this->registers.hl.w = (u16) (result & 0xFFFF);
    this->advanceCycles(8);
}

void CPU::ldi_a_hlp() {
    this->registers.af.b.h = this->gameboy->mmu->read(this->registers.hl.w++);
    this->advanceCycles(8);
}

void CPU::dec_hl() {
    this->registers.hl.w--;
    this->advanceCycles(8);
}

void CPU::inc_l() {
    u8 result = (u8) (this->registers.hl.b.l + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.b.l & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
    this->advanceCycles(4);
}

void CPU::dec_l() {
    u8 result = (u8) (this->registers.hl.b.l - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.b.l & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
    this->advanceCycles(4);
}

void CPU::ld_l_n() {
    this->registers.hl.b.l = READPC8();
    this->advanceCycles(8);
}

void CPU::cpl() {
    this->registers.af.b.h = ~this->registers.af.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, 1);
    this->advanceCycles(4);
}

void CPU::jr_nc_n() {
    s8 offset = (s8) READPC8();
    if(!FLAG_ISCARRY) {
        this->registers.pc.w += offset;
        this->advanceCycles(12);
    } else {
        this->advanceCycles(8);
    }
}

void CPU::ld_sp_nn() {
    this->registers.sp.w = READPC16();
    this->advanceCycles(12);
}

void CPU::ldd_hlp_a() {
    this->gameboy->mmu->write(this->registers.hl.w--, this->registers.af.b.h);
    this->advanceCycles(8);
}

void CPU::inc_sp() {
    this->registers.sp.w++;
    this->advanceCycles(8);
}

void CPU::inc_hlp() {
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    u8 result = (u8) (val + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (val & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->gameboy->mmu->write(this->registers.hl.w, result);
    this->advanceCycles(8);
}

void CPU::dec_hlp() {
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    u8 result = (u8) (val - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (val & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->gameboy->mmu->write(this->registers.hl.w, result);
    this->advanceCycles(8);
}

void CPU::ld_hlp_n() {
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, READPC8());
    this->advanceCycles(8);
}

void CPU::scf() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 1);
    this->advanceCycles(4);
}

void CPU::jr_c_n() {
    s8 offset = (s8) READPC8();
    if(FLAG_ISCARRY) {
        this->registers.pc.w += offset;
        this->advanceCycles(12);
    } else {
        this->advanceCycles(8);
    }
}

void CPU::add_hl_sp() {
    int result = this->registers.hl.w + this->registers.sp.w;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.w & 0xFFF) + (this->registers.sp.w & 0xFFF) >= 0x1000);
    FLAG_SET(FLAG_CARRY, result >= 0x10000);
    this->registers.hl.w = (u16) (result & 0xFFFF);
    this->advanceCycles(8);
}

void CPU::ldd_a_hlp() {
    this->registers.af.b.h = this->gameboy->mmu->read(this->registers.hl.w--);
    this->advanceCycles(8);
}

void CPU::dec_sp() {
    this->registers.sp.w--;
    this->advanceCycles(8);
}

void CPU::inc_a() {
    u8 result = (u8) (this->registers.af.b.h + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::dec_a() {
    u8 result = (u8) (this->registers.af.b.h - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::ld_a_n() {
    this->registers.af.b.h = READPC8();
    this->advanceCycles(8);
}

void CPU::ccf() {
    FLAG_SET(FLAG_CARRY, !FLAG_ISCARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    this->advanceCycles(4);
}

void CPU::ld_b_c() {
    this->registers.bc.b.h = this->registers.bc.b.l;
    this->advanceCycles(4);
}

void CPU::ld_b_d() {
    this->registers.bc.b.h = this->registers.de.b.h;
    this->advanceCycles(4);
}

void CPU::ld_b_e() {
    this->registers.bc.b.h = this->registers.de.b.l;
    this->advanceCycles(4);
}

void CPU::ld_b_h() {
    this->registers.bc.b.h = this->registers.hl.b.h;
    this->advanceCycles(4);
}

void CPU::ld_b_l() {
    this->registers.bc.b.h = this->registers.hl.b.l;
    this->advanceCycles(4);
}

void CPU::ld_b_hlp() {
    this->registers.bc.b.h = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(8);
}

void CPU::ld_b_a() {
    this->registers.bc.b.h = this->registers.af.b.h;
    this->advanceCycles(4);
}

void CPU::ld_c_b() {
    this->registers.bc.b.l = this->registers.bc.b.h;
    this->advanceCycles(4);
}

void CPU::ld_c_d() {
    this->registers.bc.b.l = this->registers.de.b.h;
    this->advanceCycles(4);
}

void CPU::ld_c_e() {
    this->registers.bc.b.l = this->registers.de.b.l;
    this->advanceCycles(4);
}

void CPU::ld_c_h() {
    this->registers.bc.b.l = this->registers.hl.b.h;
    this->advanceCycles(4);
}

void CPU::ld_c_l() {
    this->registers.bc.b.l = this->registers.hl.b.l;
    this->advanceCycles(4);
}

void CPU::ld_c_hlp() {
    this->registers.bc.b.l = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(8);
}

void CPU::ld_c_a() {
    this->registers.bc.b.l = this->registers.af.b.h;
    this->advanceCycles(4);
}

void CPU::ld_d_b() {
    this->registers.de.b.h = this->registers.bc.b.h;
    this->advanceCycles(4);
}

void CPU::ld_d_c() {
    this->registers.de.b.h = this->registers.bc.b.l;
    this->advanceCycles(4);
}

void CPU::ld_d_e() {
    this->registers.de.b.h = this->registers.de.b.l;
    this->advanceCycles(4);
}

void CPU::ld_d_h() {
    this->registers.de.b.h = this->registers.hl.b.h;
    this->advanceCycles(4);
}

void CPU::ld_d_l() {
    this->registers.de.b.h = this->registers.hl.b.l;
    this->advanceCycles(4);
}

void CPU::ld_d_hlp() {
    this->registers.de.b.h = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(8);
}

void CPU::ld_d_a() {
    this->registers.de.b.h = this->registers.af.b.h;
    this->advanceCycles(4);
}

void CPU::ld_e_b() {
    this->registers.de.b.l = this->registers.bc.b.h;
    this->advanceCycles(4);
}

void CPU::ld_e_c() {
    this->registers.de.b.l = this->registers.bc.b.l;
    this->advanceCycles(4);
}

void CPU::ld_e_d() {
    this->registers.de.b.l = this->registers.de.b.h;
    this->advanceCycles(4);
}

void CPU::ld_e_h() {
    this->registers.de.b.l = this->registers.hl.b.h;
    this->advanceCycles(4);
}

void CPU::ld_e_l() {
    this->registers.de.b.l = this->registers.hl.b.l;
    this->advanceCycles(4);
}

void CPU::ld_e_hlp() {
    this->registers.de.b.l = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(8);
}

void CPU::ld_e_a() {
    this->registers.de.b.l = this->registers.af.b.h;
    this->advanceCycles(4);
}

void CPU::ld_h_b() {
    this->registers.hl.b.h = this->registers.bc.b.h;
    this->advanceCycles(4);
}

void CPU::ld_h_c() {
    this->registers.hl.b.h = this->registers.bc.b.l;
    this->advanceCycles(4);
}

void CPU::ld_h_d() {
    this->registers.hl.b.h = this->registers.de.b.h;
    this->advanceCycles(4);
}

void CPU::ld_h_e() {
    this->registers.hl.b.h = this->registers.de.b.l;
    this->advanceCycles(4);
}

void CPU::ld_h_l() {
    this->registers.hl.b.h = this->registers.hl.b.l;
    this->advanceCycles(4);
}

void CPU::ld_h_hlp() {
    this->registers.hl.b.h = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(8);
}

void CPU::ld_h_a() {
    this->registers.hl.b.h = this->registers.af.b.h;
    this->advanceCycles(4);
}

void CPU::ld_l_b() {
    this->registers.hl.b.l = this->registers.bc.b.h;
    this->advanceCycles(4);
}

void CPU::ld_l_c() {
    this->registers.hl.b.l = this->registers.bc.b.l;
    this->advanceCycles(4);
}

void CPU::ld_l_d() {
    this->registers.hl.b.l = this->registers.de.b.h;
    this->advanceCycles(4);
}

void CPU::ld_l_e() {
    this->registers.hl.b.l = this->registers.de.b.l;
    this->advanceCycles(4);
}

void CPU::ld_l_h() {
    this->registers.hl.b.l = this->registers.hl.b.h;
    this->advanceCycles(4);
}

void CPU::ld_l_hlp() {
    this->registers.hl.b.l = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(8);
}

void CPU::ld_l_a() {
    this->registers.hl.b.l = this->registers.af.b.h;
    this->advanceCycles(4);
}

void CPU::ld_hlp_b() {
    this->gameboy->mmu->write(this->registers.hl.w, this->registers.bc.b.h);
    this->advanceCycles(8);
}

void CPU::ld_hlp_c() {
    this->gameboy->mmu->write(this->registers.hl.w, this->registers.bc.b.l);
    this->advanceCycles(8);
}

void CPU::ld_hlp_d() {
    this->gameboy->mmu->write(this->registers.hl.w, this->registers.de.b.h);
    this->advanceCycles(8);
}

void CPU::ld_hlp_e() {
    this->gameboy->mmu->write(this->registers.hl.w, this->registers.de.b.l);
    this->advanceCycles(8);
}

void CPU::ld_hlp_h() {
    this->gameboy->mmu->write(this->registers.hl.w, this->registers.hl.b.h);
    this->advanceCycles(8);
}

void CPU::ld_hlp_l() {
    this->gameboy->mmu->write(this->registers.hl.w, this->registers.hl.b.l);
    this->advanceCycles(8);
}

void CPU::halt() {
    if(!this->ime && (this->gameboy->mmu->readIO(IF) & this->gameboy->mmu->readIO(IE) & 0x1F) != 0) {
        if(this->gameboy->gbMode != MODE_CGB) {
            this->haltBug = true;
        }
    } else {
        this->haltState = true;
    }

    this->advanceCycles(4);
}

void CPU::ld_hlp_a() {
    this->gameboy->mmu->write(this->registers.hl.w, this->registers.af.b.h);
    this->advanceCycles(8);
}

void CPU::ld_a_b() {
    this->registers.af.b.h = this->registers.bc.b.h;
    this->advanceCycles(4);
}

void CPU::ld_a_c() {
    this->registers.af.b.h = this->registers.bc.b.l;
    this->advanceCycles(4);
}

void CPU::ld_a_d() {
    this->registers.af.b.h = this->registers.de.b.h;
    this->advanceCycles(4);
}

void CPU::ld_a_e() {
    this->registers.af.b.h = this->registers.de.b.l;
    this->advanceCycles(4);
}

void CPU::ld_a_h() {
    this->registers.af.b.h = this->registers.hl.b.h;
    this->advanceCycles(4);
}

void CPU::ld_a_l() {
    this->registers.af.b.h = this->registers.hl.b.l;
    this->advanceCycles(4);
}

void CPU::ld_a_hlp() {
    this->registers.af.b.h = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(8);
}

void CPU::add_a_b() {
    u16 result = this->registers.af.b.h + this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.bc.b.h & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::add_a_c() {
    u16 result = this->registers.af.b.h + this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.bc.b.l & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::add_a_d() {
    u16 result = this->registers.af.b.h + this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.de.b.h & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::add_a_e() {
    u16 result = this->registers.af.b.h + this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.de.b.l & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::add_a_h() {
    u16 result = this->registers.af.b.h + this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.hl.b.h & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::add_a_l() {
    u16 result = this->registers.af.b.h + this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.hl.b.l & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::add_a_hlp() {
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    u16 result = this->registers.af.b.h + val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (val & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(8);
}

void CPU::add_a_a() {
    u16 result = this->registers.af.b.h + this->registers.af.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.af.b.h & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::adc_b() {
    u16 result = this->registers.af.b.h + this->registers.bc.b.h + (u16) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.bc.b.h & 0xF) + FLAG_ISCARRY >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::adc_c() {
    u16 result = this->registers.af.b.h + this->registers.bc.b.l + (u16) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.bc.b.l & 0xF) + FLAG_ISCARRY >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::adc_d() {
    u16 result = this->registers.af.b.h + this->registers.de.b.h + (u16) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.de.b.h & 0xF) + FLAG_ISCARRY >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::adc_e() {
    u16 result = this->registers.af.b.h + this->registers.de.b.l + (u16) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.de.b.l & 0xF) + FLAG_ISCARRY >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::adc_h() {
    u16 result = this->registers.af.b.h + this->registers.hl.b.h + (u16) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.hl.b.h & 0xF) + FLAG_ISCARRY >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::adc_l() {
    u16 result = this->registers.af.b.h + this->registers.hl.b.l + (u16) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.hl.b.l & 0xF) + FLAG_ISCARRY >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::adc_hlp() {
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    u16 result = this->registers.af.b.h + val + (u16) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (val & 0xF) + FLAG_ISCARRY >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(8);
}

void CPU::adc_a() {
    u16 result = this->registers.af.b.h + this->registers.af.b.h + (u16) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.af.b.h & 0xF) + FLAG_ISCARRY >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sub_b() {
    int result = this->registers.af.b.h - this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sub_c() {
    int result = this->registers.af.b.h - this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sub_d() {
    int result = this->registers.af.b.h - this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sub_e() {
    int result = this->registers.af.b.h - this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sub_h() {
    int result = this->registers.af.b.h - this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sub_l() {
    int result = this->registers.af.b.h - this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sub_hlp() {
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    int result = this->registers.af.b.h - val;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(8);
}

void CPU::sub_a() {
    int result = this->registers.af.b.h - this->registers.af.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.af.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sbc_b() {
    int result = this->registers.af.b.h - this->registers.bc.b.h - FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.h & 0xF) - FLAG_ISCARRY < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sbc_c() {
    int result = this->registers.af.b.h - this->registers.bc.b.l - FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.l & 0xF) - FLAG_ISCARRY < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sbc_d() {
    int result = this->registers.af.b.h - this->registers.de.b.h - FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.h & 0xF) - FLAG_ISCARRY < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sbc_e() {
    int result = this->registers.af.b.h - this->registers.de.b.l - FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.l & 0xF) - FLAG_ISCARRY < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sbc_h() {
    int result = this->registers.af.b.h - this->registers.hl.b.h - FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.h & 0xF) - FLAG_ISCARRY < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sbc_l() {
    int result = this->registers.af.b.h - this->registers.hl.b.l - FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.l & 0xF) - FLAG_ISCARRY < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::sbc_hlp() {
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    int result = this->registers.af.b.h - val - FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) - FLAG_ISCARRY < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(8);
}

void CPU::sbc_a() {
    int result = this->registers.af.b.h - this->registers.af.b.h - FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.af.b.h & 0xF) - FLAG_ISCARRY < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(4);
}

void CPU::and_b() {
    u8 result = this->registers.af.b.h & this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::and_c() {
    u8 result = this->registers.af.b.h & this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::and_d() {
    u8 result = this->registers.af.b.h & this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::and_e() {
    u8 result = this->registers.af.b.h & this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::and_h() {
    u8 result = this->registers.af.b.h & this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::and_l() {
    u8 result = this->registers.af.b.h & this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::and_hlp() {
    u8 result = this->registers.af.b.h & this->gameboy->mmu->read(this->registers.hl.w);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::and_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, this->registers.af.b.h == 0);
    this->advanceCycles(4);
}

void CPU::xor_b() {
    u8 result = this->registers.af.b.h ^ this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::xor_c() {
    u8 result = this->registers.af.b.h ^ this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::xor_d() {
    u8 result = this->registers.af.b.h ^ this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::xor_e() {
    u8 result = this->registers.af.b.h ^ this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::xor_h() {
    u8 result = this->registers.af.b.h ^ this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::xor_l() {
    u8 result = this->registers.af.b.h ^ this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::xor_hlp() {
    u8 result = this->registers.af.b.h ^this->gameboy->mmu->read(this->registers.hl.w);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::xor_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, 1);
    this->registers.af.b.h = 0;
    this->advanceCycles(4);
}

void CPU::or_b() {
    u8 result = this->registers.af.b.h | this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::or_c() {
    u8 result = this->registers.af.b.h | this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::or_d() {
    u8 result = this->registers.af.b.h | this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::or_e() {
    u8 result = this->registers.af.b.h | this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::or_h() {
    u8 result = this->registers.af.b.h | this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::or_l() {
    u8 result = this->registers.af.b.h | this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(4);
}

void CPU::or_hlp() {
    u8 result = this->registers.af.b.h | this->gameboy->mmu->read(this->registers.hl.w);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::or_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, this->registers.af.b.h == 0);
    this->advanceCycles(4);
}

void CPU::cp_b() {
    int result = this->registers.af.b.h - this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->advanceCycles(4);
}

void CPU::cp_c() {
    int result = this->registers.af.b.h - this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->advanceCycles(4);
}

void CPU::cp_d() {
    int result = this->registers.af.b.h - this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->advanceCycles(4);
}

void CPU::cp_e() {
    int result = this->registers.af.b.h - this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->advanceCycles(4);
}

void CPU::cp_h() {
    int result = this->registers.af.b.h - this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->advanceCycles(4);
}

void CPU::cp_l() {
    int result = this->registers.af.b.h - this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->advanceCycles(4);
}

void CPU::cp_hlp() {
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    int result = this->registers.af.b.h - val;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->advanceCycles(8);
}

void CPU::cp_a() {
    int result = this->registers.af.b.h - this->registers.af.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.af.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->advanceCycles(4);
}

void CPU::ret_nz() {
    if(!FLAG_ISZERO) {
        POP(this->registers.pc.w);
        this->advanceCycles(20);
    } else {
        this->advanceCycles(8);
    }
}

void CPU::pop_bc() {
    POP(this->registers.bc.w);
    this->advanceCycles(12);
}

void CPU::jp_nz_nn() {
    u16 addr = READPC16();
    if(!FLAG_ISZERO) {
        this->registers.pc.w = addr;
        this->advanceCycles(16);
    } else {
        this->advanceCycles(12);
    }
}

void CPU::jp_nn() {
    this->registers.pc.w = READPC16();
    this->advanceCycles(16);
}

void CPU::call_nz_nn() {
    u16 addr = READPC16();
    if(!FLAG_ISZERO) {
        PUSH(this->registers.pc.w);
        this->registers.pc.w = addr;
        this->advanceCycles(24);
    } else {
        this->advanceCycles(12);
    }
}

void CPU::push_bc() {
    PUSH(this->registers.bc.w);
    this->advanceCycles(16);
}

void CPU::add_a_n() {
    u8 val = READPC8();
    u16 result = this->registers.af.b.h + val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (val & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(8);
}

void CPU::rst_0() {
    PUSH(this->registers.pc.w);
    this->registers.pc.w = 0x0000;
    this->advanceCycles(16);
}

void CPU::ret_z() {
    if(FLAG_ISZERO) {
        POP(this->registers.pc.w);
        this->advanceCycles(20);
    } else {
        this->advanceCycles(8);
    }
}

void CPU::ret() {
    POP(this->registers.pc.w);
    this->advanceCycles(16);
}

void CPU::jp_z_nn() {
    u16 addr = READPC16();
    if(FLAG_ISZERO) {
        this->registers.pc.w = addr;
        this->advanceCycles(16);
    } else {
        this->advanceCycles(12);
    }
}

void CPU::cb_n() {
    (this->*cbOpcodes[READPC8()])();
}

void CPU::call_z_nn() {
    u16 addr = READPC16();
    if(FLAG_ISZERO) {
        PUSH(this->registers.pc.w);
        this->registers.pc.w = addr;
        this->advanceCycles(24);
    } else {
        this->advanceCycles(12);
    }
}

void CPU::call_nn() {
    u16 addr = READPC16();
    PUSH(this->registers.pc.w);
    this->registers.pc.w = addr;
    this->advanceCycles(24);
}

void CPU::adc_n() {
    u8 val = READPC8();
    u16 result = this->registers.af.b.h + val + (u16) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (val & 0xF) + FLAG_ISCARRY >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(8);
}

void CPU::rst_08() {
    PUSH(this->registers.pc.w);
    this->registers.pc.w = 0x0008;
    this->advanceCycles(16);
}

void CPU::ret_nc() {
    if(!FLAG_ISCARRY) {
        POP(this->registers.pc.w);
        this->advanceCycles(20);
    } else {
        this->advanceCycles(8);
    }
}

void CPU::pop_de() {
    POP(this->registers.de.w);
    this->advanceCycles(12);
}

void CPU::jp_nc_nn() {
    u16 addr = READPC16();
    if(!FLAG_ISCARRY) {
        this->registers.pc.w = addr;
        this->advanceCycles(16);
    } else {
        this->advanceCycles(12);
    }
}

void CPU::call_nc_nn() {
    u16 addr = READPC16();
    if(!FLAG_ISCARRY) {
        PUSH(this->registers.pc.w);
        this->registers.pc.w = addr;
        this->advanceCycles(24);
    } else {
        this->advanceCycles(12);
    }
}

void CPU::push_de() {
    PUSH(this->registers.de.w);
    this->advanceCycles(16);
}

void CPU::sub_n() {
    u8 val = READPC8();
    int result = this->registers.af.b.h - val;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(8);
}

void CPU::rst_10() {
    PUSH(this->registers.pc.w);
    this->registers.pc.w = 0x0010;
    this->advanceCycles(16);
}

void CPU::ret_c() {
    if(FLAG_ISCARRY) {
        POP(this->registers.pc.w);
        this->advanceCycles(20);
    } else {
        this->advanceCycles(8);
    }
}

void CPU::reti() {
    POP(this->registers.pc.w);
    this->ime = true;
    this->advanceCycles(16);
}

void CPU::jp_c_nn() {
    u16 addr = READPC16();
    if(FLAG_ISCARRY) {
        this->registers.pc.w = addr;
        this->advanceCycles(16);
    } else {
        this->advanceCycles(12);
    }
}

void CPU::call_c_nn() {
    u16 addr = READPC16();
    if(FLAG_ISCARRY) {
        PUSH(this->registers.pc.w);
        this->registers.pc.w = addr;
        this->advanceCycles(24);
    } else {
        this->advanceCycles(12);
    }
}

void CPU::sbc_n() {
    u8 val = READPC8();
    int result = this->registers.af.b.h - val - FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) - FLAG_ISCARRY < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
    this->advanceCycles(8);
}

void CPU::rst_18() {
    PUSH(this->registers.pc.w);
    this->registers.pc.w = 0x0018;
    this->advanceCycles(16);
}

void CPU::ld_ff_n_a() {
    this->advanceCycles(4);
    this->gameboy->mmu->write((u16) (0xFF00 + READPC8()), this->registers.af.b.h);
    this->advanceCycles(8);
}

void CPU::pop_hl() {
    POP(this->registers.hl.w);
    this->advanceCycles(12);
}

void CPU::ld_ff_c_a() {
    this->gameboy->mmu->write((u16) (0xFF00 + this->registers.bc.b.l), this->registers.af.b.h);
    this->advanceCycles(8);
}

void CPU::push_hl() {
    PUSH(this->registers.hl.w);
    this->advanceCycles(16);
}

void CPU::and_n() {
    u8 result = this->registers.af.b.h & READPC8();
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::rst_20() {
    PUSH(this->registers.pc.w);
    this->registers.pc.w = 0x0020;
    this->advanceCycles(16);
}

void CPU::add_sp_n() {
    u8 val = READPC8();
    u16 result = this->registers.sp.w + (s8) val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.sp.w & 0xF) + (val & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, (this->registers.sp.w & 0xFF) + val >= 0x100);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.sp.w = result;
    this->advanceCycles(16);
}

void CPU::jp_hl() {
    this->registers.pc.w = this->registers.hl.w;
    this->advanceCycles(4);
}

void CPU::ld_nnp_a() {
    this->advanceCycles(8);
    this->gameboy->mmu->write(READPC16(), this->registers.af.b.h);
    this->advanceCycles(8);
}

void CPU::xor_n() {
    u8 result = this->registers.af.b.h ^READPC8();
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::rst_28() {
    PUSH(this->registers.pc.w);
    this->registers.pc.w = 0x0028;
    this->advanceCycles(16);
}

void CPU::ld_a_ff_n() {
    this->advanceCycles(4);
    this->registers.af.b.h = this->gameboy->mmu->read((u16) (0xFF00 + READPC8()));
    this->advanceCycles(8);
}

void CPU::pop_af() {
    u8 oldFLow = (u8) (this->registers.af.b.l & 0xF);
    POP(this->registers.af.w);
    this->registers.af.b.l = (u8) ((this->registers.af.b.l & 0xF0) | oldFLow);
    this->advanceCycles(12);
}

void CPU::ld_a_ff_c() {
    this->registers.af.b.h = this->gameboy->mmu->read((u16) (0xFF00 + this->registers.bc.b.l));
    this->advanceCycles(8);
}

void CPU::di_inst() {
    this->ime = false;
    this->advanceCycles(4);
}

void CPU::push_af() {
    PUSH(this->registers.af.w);
    this->advanceCycles(16);
}

void CPU::or_n() {
    u8 result = this->registers.af.b.h | READPC8();
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::rst_30() {
    PUSH(this->registers.pc.w);
    this->registers.pc.w = 0x0030;
    this->advanceCycles(16);
}

void CPU::ld_hl_sp_n() {
    u8 val = READPC8();
    u16 result = this->registers.sp.w + (s8) val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.sp.w & 0xF) + (val & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, (this->registers.sp.w & 0xFF) + val >= 0x100);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.hl.w = result;
    this->advanceCycles(12);
}

void CPU::ld_sp_hl() {
    this->registers.sp.w = this->registers.hl.w;
    this->advanceCycles(8);
}

void CPU::ld_a_nnp() {
    this->advanceCycles(8);
    this->registers.af.b.h = this->gameboy->mmu->read(READPC16());
    this->advanceCycles(8);
}

void CPU::ei() {
    this->ime = true;
    this->advanceCycles(4);
}

void CPU::cp_n() {
    u8 val = READPC8();
    int result = this->registers.af.b.h - val;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->advanceCycles(8);
}

void CPU::rst_38() {
    PUSH(this->registers.pc.w);
    this->registers.pc.w = 0x0038;
    this->advanceCycles(16);
}

void CPU::rlc_b() {
    u8 carry = (u8) ((this->registers.bc.b.h & 0x80) >> 7);
    u8 result = (this->registers.bc.b.h << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
    this->advanceCycles(8);
}

void CPU::rlc_c() {
    u8 carry = (u8) ((this->registers.bc.b.l & 0x80) >> 7);
    u8 result = (this->registers.bc.b.l << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
    this->advanceCycles(8);
}

void CPU::rlc_d() {
    u8 carry = (u8) ((this->registers.de.b.h & 0x80) >> 7);
    u8 result = (this->registers.de.b.h << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
    this->advanceCycles(8);
}

void CPU::rlc_e() {
    u8 carry = (u8) ((this->registers.de.b.l & 0x80) >> 7);
    u8 result = (this->registers.de.b.l << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
    this->advanceCycles(8);
}

void CPU::rlc_h() {
    u8 carry = (u8) ((this->registers.hl.b.h & 0x80) >> 7);
    u8 result = (this->registers.hl.b.h << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
    this->advanceCycles(8);
}

void CPU::rlc_l() {
    u8 carry = (u8) ((this->registers.hl.b.l & 0x80) >> 7);
    u8 result = (this->registers.hl.b.l << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
    this->advanceCycles(8);
}

void CPU::rlc_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    u8 carry = (u8) ((val & 0x80) >> 7);
    u8 result = (val << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->gameboy->mmu->write(this->registers.hl.w, result);
    this->advanceCycles(8);
}

void CPU::rlc_a() {
    u8 carry = (u8) ((this->registers.af.b.h & 0x80) >> 7);
    u8 result = (this->registers.af.b.h << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::rrc_b() {
    u8 carry = (u8) ((this->registers.bc.b.h & 0x01) << 7);
    u8 result = (this->registers.bc.b.h >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
    this->advanceCycles(8);
}

void CPU::rrc_c() {
    u8 carry = (u8) ((this->registers.bc.b.l & 0x01) << 7);
    u8 result = (this->registers.bc.b.l >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
    this->advanceCycles(8);
}

void CPU::rrc_d() {
    u8 carry = (u8) ((this->registers.de.b.h & 0x01) << 7);
    u8 result = (this->registers.de.b.h >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
    this->advanceCycles(8);
}

void CPU::rrc_e() {
    u8 carry = (u8) ((this->registers.de.b.l & 0x01) << 7);
    u8 result = (this->registers.de.b.l >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
    this->advanceCycles(8);
}

void CPU::rrc_h() {
    u8 carry = (u8) ((this->registers.hl.b.h & 0x01) << 7);
    u8 result = (this->registers.hl.b.h >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
    this->advanceCycles(8);
}

void CPU::rrc_l() {
    u8 carry = (u8) ((this->registers.hl.b.l & 0x01) << 7);
    u8 result = (this->registers.hl.b.l >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
    this->advanceCycles(8);
}

void CPU::rrc_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    u8 carry = (u8) ((val & 0x01) << 7);
    u8 result = (val >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->gameboy->mmu->write(this->registers.hl.w, result);
    this->advanceCycles(8);
}

void CPU::rrc_a() {
    u8 carry = (u8) ((this->registers.af.b.h & 0x01) << 7);
    u8 result = (this->registers.af.b.h >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::rl_b() {
    u8 result = (this->registers.bc.b.h << 1) | (u8) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
    this->advanceCycles(8);
}

void CPU::rl_c() {
    u8 result = (this->registers.bc.b.l << 1) | (u8) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
    this->advanceCycles(8);
}

void CPU::rl_d() {
    u8 result = (this->registers.de.b.h << 1) | (u8) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
    this->advanceCycles(8);
}

void CPU::rl_e() {
    u8 result = (this->registers.de.b.l << 1) | (u8) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
    this->advanceCycles(8);
}

void CPU::rl_h() {
    u8 result = (this->registers.hl.b.h << 1) | (u8) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
    this->advanceCycles(8);
}

void CPU::rl_l() {
    u8 result = (this->registers.hl.b.l << 1) | (u8) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
    this->advanceCycles(8);
}

void CPU::rl_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    u8 result = (val << 1) | (u8) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (val & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->gameboy->mmu->write(this->registers.hl.w, result);
    this->advanceCycles(8);
}

void CPU::rl_a() {
    u8 result = (this->registers.af.b.h << 1) | (u8) FLAG_ISCARRY;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::rr_b() {
    u8 result = (this->registers.bc.b.h >> 1) | (u8) (FLAG_ISCARRY << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
    this->advanceCycles(8);
}

void CPU::rr_c() {
    u8 result = (this->registers.bc.b.l >> 1) | (u8) (FLAG_ISCARRY << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
    this->advanceCycles(8);
}

void CPU::rr_d() {
    u8 result = (this->registers.de.b.h >> 1) | (u8) (FLAG_ISCARRY << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
    this->advanceCycles(8);
}

void CPU::rr_e() {
    u8 result = (this->registers.de.b.l >> 1) | (u8) (FLAG_ISCARRY << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
    this->advanceCycles(8);
}

void CPU::rr_h() {
    u8 result = (this->registers.hl.b.h >> 1) | (u8) (FLAG_ISCARRY << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
    this->advanceCycles(8);
}

void CPU::rr_l() {
    u8 result = (this->registers.hl.b.l >> 1) | (u8) (FLAG_ISCARRY << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
    this->advanceCycles(8);
}

void CPU::rr_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    u8 result = (val >> 1) | (u8) (FLAG_ISCARRY << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (val & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->gameboy->mmu->write(this->registers.hl.w, result);
    this->advanceCycles(8);
}

void CPU::rr_a() {
    u8 result = (this->registers.af.b.h >> 1) | (u8) (FLAG_ISCARRY << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::sla_b() {
    u8 result = this->registers.bc.b.h << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
    this->advanceCycles(8);
}

void CPU::sla_c() {
    u8 result = this->registers.bc.b.l << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
    this->advanceCycles(8);
}

void CPU::sla_d() {
    u8 result = this->registers.de.b.h << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
    this->advanceCycles(8);
}

void CPU::sla_e() {
    u8 result = this->registers.de.b.l << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
    this->advanceCycles(8);
}

void CPU::sla_h() {
    u8 result = this->registers.hl.b.h << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
    this->advanceCycles(8);
}

void CPU::sla_l() {
    u8 result = this->registers.hl.b.l << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
    this->advanceCycles(8);
}

void CPU::sla_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    u8 result = val << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (val & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->gameboy->mmu->write(this->registers.hl.w, result);
    this->advanceCycles(8);
}

void CPU::sla_a() {
    u8 result = this->registers.af.b.h << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::sra_b() {
    u8 result = (this->registers.bc.b.h >> 1) | (u8) (this->registers.bc.b.h & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
    this->advanceCycles(8);
}

void CPU::sra_c() {
    u8 result = (this->registers.bc.b.l >> 1) | (u8) (this->registers.bc.b.l & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
    this->advanceCycles(8);
}

void CPU::sra_d() {
    u8 result = (this->registers.de.b.h >> 1) | (u8) (this->registers.de.b.h & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
    this->advanceCycles(8);
}

void CPU::sra_e() {
    u8 result = (this->registers.de.b.l >> 1) | (u8) (this->registers.de.b.l & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
    this->advanceCycles(8);
}

void CPU::sra_h() {
    u8 result = (this->registers.hl.b.h >> 1) | (u8) (this->registers.hl.b.h & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
    this->advanceCycles(8);
}

void CPU::sra_l() {
    u8 result = (this->registers.hl.b.l >> 1) | (u8) (this->registers.hl.b.l & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
    this->advanceCycles(8);
}

void CPU::sra_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    u8 result = (val >> 1) | (u8) (val & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (val & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->gameboy->mmu->write(this->registers.hl.w, result);
    this->advanceCycles(8);
}

void CPU::sra_a() {
    u8 result = (this->registers.af.b.h >> 1) | (u8) (this->registers.af.b.h & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::swap_b() {
    u8 result = (u8) (((this->registers.bc.b.h & 0x0F) << 4) | ((this->registers.bc.b.h & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
    this->advanceCycles(8);
}

void CPU::swap_c() {
    u8 result = (u8) (((this->registers.bc.b.l & 0x0F) << 4) | ((this->registers.bc.b.l & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
    this->advanceCycles(8);
}

void CPU::swap_d() {
    u8 result = (u8) (((this->registers.de.b.h & 0x0F) << 4) | ((this->registers.de.b.h & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
    this->advanceCycles(8);
}

void CPU::swap_e() {
    u8 result = (u8) (((this->registers.de.b.l & 0x0F) << 4) | ((this->registers.de.b.l & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
    this->advanceCycles(8);
}

void CPU::swap_h() {
    u8 result = (u8) (((this->registers.hl.b.h & 0x0F) << 4) | ((this->registers.hl.b.h & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
    this->advanceCycles(8);
}

void CPU::swap_l() {
    u8 result = (u8) (((this->registers.hl.b.l & 0x0F) << 4) | ((this->registers.hl.b.l & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
    this->advanceCycles(8);
}

void CPU::swap_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    u8 result = (u8) (((val & 0x0F) << 4) | ((val & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->gameboy->mmu->write(this->registers.hl.w, result);
    this->advanceCycles(8);
}

void CPU::swap_a() {
    u8 result = (u8) (((this->registers.af.b.h & 0x0F) << 4) | ((this->registers.af.b.h & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::srl_b() {
    u8 result = this->registers.bc.b.h >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
    this->advanceCycles(8);
}

void CPU::srl_c() {
    u8 result = this->registers.bc.b.l >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
    this->advanceCycles(8);
}

void CPU::srl_d() {
    u8 result = this->registers.de.b.h >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
    this->advanceCycles(8);
}

void CPU::srl_e() {
    u8 result = this->registers.de.b.l >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
    this->advanceCycles(8);
}

void CPU::srl_h() {
    u8 result = this->registers.hl.b.h >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
    this->advanceCycles(8);
}

void CPU::srl_l() {
    u8 result = this->registers.hl.b.l >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
    this->advanceCycles(8);
}

void CPU::srl_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    u8 result = val >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (val & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->gameboy->mmu->write(this->registers.hl.w, result);
    this->advanceCycles(8);
}

void CPU::srl_a() {
    u8 result = this->registers.af.b.h >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
    this->advanceCycles(8);
}

void CPU::bit_0_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 0)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_0_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 0)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_0_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 0)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_0_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 0)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_0_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 0)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_0_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 0)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_0_hlp() {
    this->advanceCycles(4);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->gameboy->mmu->read(this->registers.hl.w) & (1 << 0)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_0_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 0)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_1_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 1)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_1_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 1)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_1_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 1)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_1_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 1)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_1_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 1)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_1_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 1)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_1_hlp() {
    this->advanceCycles(4);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->gameboy->mmu->read(this->registers.hl.w) & (1 << 1)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_1_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 1)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_2_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 2)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_2_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 2)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_2_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 2)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_2_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 2)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_2_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 2)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_2_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 2)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_2_hlp() {
    this->advanceCycles(4);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->gameboy->mmu->read(this->registers.hl.w) & (1 << 2)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_2_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 2)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_3_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 3)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_3_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 3)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_3_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 3)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_3_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 3)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_3_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 3)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_3_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 3)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_3_hlp() {
    this->advanceCycles(4);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->gameboy->mmu->read(this->registers.hl.w) & (1 << 3)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_3_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 3)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_4_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 4)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_4_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 4)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_4_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 4)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_4_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 4)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_4_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 4)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_4_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 4)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_4_hlp() {
    this->advanceCycles(4);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->gameboy->mmu->read(this->registers.hl.w) & (1 << 4)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_4_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 4)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_5_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 5)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_5_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 5)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_5_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 5)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_5_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 5)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_5_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 5)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_5_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 5)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_5_hlp() {
    this->advanceCycles(4);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->gameboy->mmu->read(this->registers.hl.w) & (1 << 5)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_5_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 5)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_6_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 6)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_6_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 6)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_6_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 6)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_6_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 6)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_6_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 6)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_6_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 6)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_6_hlp() {
    this->advanceCycles(4);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->gameboy->mmu->read(this->registers.hl.w) & (1 << 6)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_6_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 6)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_7_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 7)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_7_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 7)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_7_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 7)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_7_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 7)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_7_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 7)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_7_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 7)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_7_hlp() {
    this->advanceCycles(4);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->gameboy->mmu->read(this->registers.hl.w) & (1 << 7)) == 0);
    this->advanceCycles(8);
}

void CPU::bit_7_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 7)) == 0);
    this->advanceCycles(8);
}

void CPU::res_0_b() {
    this->registers.bc.b.h &= ~(1 << 0);
    this->advanceCycles(8);
}

void CPU::res_0_c() {
    this->registers.bc.b.l &= ~(1 << 0);
    this->advanceCycles(8);
}

void CPU::res_0_d() {
    this->registers.de.b.h &= ~(1 << 0);
    this->advanceCycles(8);
}

void CPU::res_0_e() {
    this->registers.de.b.l &= ~(1 << 0);
    this->advanceCycles(8);
}

void CPU::res_0_h() {
    this->registers.hl.b.h &= ~(1 << 0);
    this->advanceCycles(8);
}

void CPU::res_0_l() {
    this->registers.hl.b.l &= ~(1 << 0);
    this->advanceCycles(8);
}

void CPU::res_0_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val & (u8) ~(1 << 0));
    this->advanceCycles(8);
}

void CPU::res_0_a() {
    this->registers.af.b.h &= ~(1 << 0);
    this->advanceCycles(8);
}

void CPU::res_1_b() {
    this->registers.bc.b.h &= ~(1 << 1);
    this->advanceCycles(8);
}

void CPU::res_1_c() {
    this->registers.bc.b.l &= ~(1 << 1);
    this->advanceCycles(8);
}

void CPU::res_1_d() {
    this->registers.de.b.h &= ~(1 << 1);
    this->advanceCycles(8);
}

void CPU::res_1_e() {
    this->registers.de.b.l &= ~(1 << 1);
    this->advanceCycles(8);
}

void CPU::res_1_h() {
    this->registers.hl.b.h &= ~(1 << 1);
    this->advanceCycles(8);
}

void CPU::res_1_l() {
    this->registers.hl.b.l &= ~(1 << 1);
    this->advanceCycles(8);
}

void CPU::res_1_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val & (u8) ~(1 << 1));
    this->advanceCycles(8);
}

void CPU::res_1_a() {
    this->registers.af.b.h &= ~(1 << 1);
    this->advanceCycles(8);
}

void CPU::res_2_b() {
    this->registers.bc.b.h &= ~(1 << 2);
    this->advanceCycles(8);
}

void CPU::res_2_c() {
    this->registers.bc.b.l &= ~(1 << 2);
    this->advanceCycles(8);
}

void CPU::res_2_d() {
    this->registers.de.b.h &= ~(1 << 2);
    this->advanceCycles(8);
}

void CPU::res_2_e() {
    this->registers.de.b.l &= ~(1 << 2);
    this->advanceCycles(8);
}

void CPU::res_2_h() {
    this->registers.hl.b.h &= ~(1 << 2);
    this->advanceCycles(8);
}

void CPU::res_2_l() {
    this->registers.hl.b.l &= ~(1 << 2);
    this->advanceCycles(8);
}

void CPU::res_2_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val & (u8) ~(1 << 2));
    this->advanceCycles(8);
}

void CPU::res_2_a() {
    this->registers.af.b.h &= ~(1 << 2);
    this->advanceCycles(8);
}

void CPU::res_3_b() {
    this->registers.bc.b.h &= ~(1 << 3);
    this->advanceCycles(8);
}

void CPU::res_3_c() {
    this->registers.bc.b.l &= ~(1 << 3);
    this->advanceCycles(8);
}

void CPU::res_3_d() {
    this->registers.de.b.h &= ~(1 << 3);
    this->advanceCycles(8);
}

void CPU::res_3_e() {
    this->registers.de.b.l &= ~(1 << 3);
    this->advanceCycles(8);
}

void CPU::res_3_h() {
    this->registers.hl.b.h &= ~(1 << 3);
    this->advanceCycles(8);
}

void CPU::res_3_l() {
    this->registers.hl.b.l &= ~(1 << 3);
    this->advanceCycles(8);
}

void CPU::res_3_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val & (u8) ~(1 << 3));
    this->advanceCycles(8);
}

void CPU::res_3_a() {
    this->registers.af.b.h &= ~(1 << 3);
    this->advanceCycles(8);
}

void CPU::res_4_b() {
    this->registers.bc.b.h &= ~(1 << 4);
    this->advanceCycles(8);
}

void CPU::res_4_c() {
    this->registers.bc.b.l &= ~(1 << 4);
    this->advanceCycles(8);
}

void CPU::res_4_d() {
    this->registers.de.b.h &= ~(1 << 4);
    this->advanceCycles(8);
}

void CPU::res_4_e() {
    this->registers.de.b.l &= ~(1 << 4);
    this->advanceCycles(8);
}

void CPU::res_4_h() {
    this->registers.hl.b.h &= ~(1 << 4);
    this->advanceCycles(8);
}

void CPU::res_4_l() {
    this->registers.hl.b.l &= ~(1 << 4);
    this->advanceCycles(8);
}

void CPU::res_4_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val & (u8) ~(1 << 4));
    this->advanceCycles(8);
}

void CPU::res_4_a() {
    this->registers.af.b.h &= ~(1 << 4);
    this->advanceCycles(8);
}

void CPU::res_5_b() {
    this->registers.bc.b.h &= ~(1 << 5);
    this->advanceCycles(8);
}

void CPU::res_5_c() {
    this->registers.bc.b.l &= ~(1 << 5);
    this->advanceCycles(8);
}

void CPU::res_5_d() {
    this->registers.de.b.h &= ~(1 << 5);
    this->advanceCycles(8);
}

void CPU::res_5_e() {
    this->registers.de.b.l &= ~(1 << 5);
    this->advanceCycles(8);
}

void CPU::res_5_h() {
    this->registers.hl.b.h &= ~(1 << 5);
    this->advanceCycles(8);
}

void CPU::res_5_l() {
    this->registers.hl.b.l &= ~(1 << 5);
    this->advanceCycles(8);
}

void CPU::res_5_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val & (u8) ~(1 << 5));
    this->advanceCycles(8);
}

void CPU::res_5_a() {
    this->registers.af.b.h &= ~(1 << 5);
    this->advanceCycles(8);
}

void CPU::res_6_b() {
    this->registers.bc.b.h &= ~(1 << 6);
    this->advanceCycles(8);
}

void CPU::res_6_c() {
    this->registers.bc.b.l &= ~(1 << 6);
    this->advanceCycles(8);
}

void CPU::res_6_d() {
    this->registers.de.b.h &= ~(1 << 6);
    this->advanceCycles(8);
}

void CPU::res_6_e() {
    this->registers.de.b.l &= ~(1 << 6);
    this->advanceCycles(8);
}

void CPU::res_6_h() {
    this->registers.hl.b.h &= ~(1 << 6);
    this->advanceCycles(8);
}

void CPU::res_6_l() {
    this->registers.hl.b.l &= ~(1 << 6);
    this->advanceCycles(8);
}

void CPU::res_6_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val & (u8) ~(1 << 6));
    this->advanceCycles(8);
}

void CPU::res_6_a() {
    this->registers.af.b.h &= ~(1 << 6);
    this->advanceCycles(8);
}

void CPU::res_7_b() {
    this->registers.bc.b.h &= ~(1 << 7);
    this->advanceCycles(8);
}

void CPU::res_7_c() {
    this->registers.bc.b.l &= ~(1 << 7);
    this->advanceCycles(8);
}

void CPU::res_7_d() {
    this->registers.de.b.h &= ~(1 << 7);
    this->advanceCycles(8);
}

void CPU::res_7_e() {
    this->registers.de.b.l &= ~(1 << 7);
    this->advanceCycles(8);
}

void CPU::res_7_h() {
    this->registers.hl.b.h &= ~(1 << 7);
    this->advanceCycles(8);
}

void CPU::res_7_l() {
    this->registers.hl.b.l &= ~(1 << 7);
    this->advanceCycles(8);
}

void CPU::res_7_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val & (u8) ~(1 << 7));
    this->advanceCycles(8);
}

void CPU::res_7_a() {
    this->registers.af.b.h &= ~(1 << 7);
    this->advanceCycles(8);
}

void CPU::set_0_b() {
    this->registers.bc.b.h |= 1 << 0;
    this->advanceCycles(8);
}

void CPU::set_0_c() {
    this->registers.bc.b.l |= 1 << 0;
    this->advanceCycles(8);
}

void CPU::set_0_d() {
    this->registers.de.b.h |= 1 << 0;
    this->advanceCycles(8);
}

void CPU::set_0_e() {
    this->registers.de.b.l |= 1 << 0;
    this->advanceCycles(8);
}

void CPU::set_0_h() {
    this->registers.hl.b.h |= 1 << 0;
    this->advanceCycles(8);
}

void CPU::set_0_l() {
    this->registers.hl.b.l |= 1 << 0;
    this->advanceCycles(8);
}

void CPU::set_0_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val | (u8) (1 << 0));
    this->advanceCycles(8);
}

void CPU::set_0_a() {
    this->registers.af.b.h |= 1 << 0;
    this->advanceCycles(8);
}

void CPU::set_1_b() {
    this->registers.bc.b.h |= 1 << 1;
    this->advanceCycles(8);
}

void CPU::set_1_c() {
    this->registers.bc.b.l |= 1 << 1;
    this->advanceCycles(8);
}

void CPU::set_1_d() {
    this->registers.de.b.h |= 1 << 1;
    this->advanceCycles(8);
}

void CPU::set_1_e() {
    this->registers.de.b.l |= 1 << 1;
    this->advanceCycles(8);
}

void CPU::set_1_h() {
    this->registers.hl.b.h |= 1 << 1;
    this->advanceCycles(8);
}

void CPU::set_1_l() {
    this->registers.hl.b.l |= 1 << 1;
    this->advanceCycles(8);
}

void CPU::set_1_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val | (u8) (1 << 1));
    this->advanceCycles(8);
}

void CPU::set_1_a() {
    this->registers.af.b.h |= 1 << 1;
    this->advanceCycles(8);
}

void CPU::set_2_b() {
    this->registers.bc.b.h |= 1 << 2;
    this->advanceCycles(8);
}

void CPU::set_2_c() {
    this->registers.bc.b.l |= 1 << 2;
    this->advanceCycles(8);
}

void CPU::set_2_d() {
    this->registers.de.b.h |= 1 << 2;
    this->advanceCycles(8);
}

void CPU::set_2_e() {
    this->registers.de.b.l |= 1 << 2;
    this->advanceCycles(8);
}

void CPU::set_2_h() {
    this->registers.hl.b.h |= 1 << 2;
    this->advanceCycles(8);
}

void CPU::set_2_l() {
    this->registers.hl.b.l |= 1 << 2;
    this->advanceCycles(8);
}

void CPU::set_2_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val | (u8) (1 << 2));
    this->advanceCycles(8);
}

void CPU::set_2_a() {
    this->registers.af.b.h |= 1 << 2;
    this->advanceCycles(8);
}

void CPU::set_3_b() {
    this->registers.bc.b.h |= 1 << 3;
    this->advanceCycles(8);
}

void CPU::set_3_c() {
    this->registers.bc.b.l |= 1 << 3;
    this->advanceCycles(8);
}

void CPU::set_3_d() {
    this->registers.de.b.h |= 1 << 3;
    this->advanceCycles(8);
}

void CPU::set_3_e() {
    this->registers.de.b.l |= 1 << 3;
    this->advanceCycles(8);
}

void CPU::set_3_h() {
    this->registers.hl.b.h |= 1 << 3;
    this->advanceCycles(8);
}

void CPU::set_3_l() {
    this->registers.hl.b.l |= 1 << 3;
    this->advanceCycles(8);
}

void CPU::set_3_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val | (u8) (1 << 3));
    this->advanceCycles(8);
}

void CPU::set_3_a() {
    this->registers.af.b.h |= 1 << 3;
    this->advanceCycles(8);
}

void CPU::set_4_b() {
    this->registers.bc.b.h |= 1 << 4;
    this->advanceCycles(8);
}

void CPU::set_4_c() {
    this->registers.bc.b.l |= 1 << 4;
    this->advanceCycles(8);
}

void CPU::set_4_d() {
    this->registers.de.b.h |= 1 << 4;
    this->advanceCycles(8);
}

void CPU::set_4_e() {
    this->registers.de.b.l |= 1 << 4;
    this->advanceCycles(8);
}

void CPU::set_4_h() {
    this->registers.hl.b.h |= 1 << 4;
    this->advanceCycles(8);
}

void CPU::set_4_l() {
    this->registers.hl.b.l |= 1 << 4;
    this->advanceCycles(8);
}

void CPU::set_4_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val | (u8) (1 << 4));
    this->advanceCycles(8);
}

void CPU::set_4_a() {
    this->registers.af.b.h |= 1 << 4;
    this->advanceCycles(8);
}

void CPU::set_5_b() {
    this->registers.bc.b.h |= 1 << 5;
    this->advanceCycles(8);
}

void CPU::set_5_c() {
    this->registers.bc.b.l |= 1 << 5;
    this->advanceCycles(8);
}

void CPU::set_5_d() {
    this->registers.de.b.h |= 1 << 5;
    this->advanceCycles(8);
}

void CPU::set_5_e() {
    this->registers.de.b.l |= 1 << 5;
    this->advanceCycles(8);
}

void CPU::set_5_h() {
    this->registers.hl.b.h |= 1 << 5;
    this->advanceCycles(8);
}

void CPU::set_5_l() {
    this->registers.hl.b.l |= 1 << 5;
    this->advanceCycles(8);
}

void CPU::set_5_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val | (u8) (1 << 5));
    this->advanceCycles(8);
}

void CPU::set_5_a() {
    this->registers.af.b.h |= 1 << 5;
    this->advanceCycles(8);
}

void CPU::set_6_b() {
    this->registers.bc.b.h |= 1 << 6;
    this->advanceCycles(8);
}

void CPU::set_6_c() {
    this->registers.bc.b.l |= 1 << 6;
    this->advanceCycles(8);
}

void CPU::set_6_d() {
    this->registers.de.b.h |= 1 << 6;
    this->advanceCycles(8);
}

void CPU::set_6_e() {
    this->registers.de.b.l |= 1 << 6;
    this->advanceCycles(8);
}

void CPU::set_6_h() {
    this->registers.hl.b.h |= 1 << 6;
    this->advanceCycles(8);
}

void CPU::set_6_l() {
    this->registers.hl.b.l |= 1 << 6;
    this->advanceCycles(8);
}

void CPU::set_6_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val | (u8) (1 << 6));
    this->advanceCycles(8);
}

void CPU::set_6_a() {
    this->registers.af.b.h |= 1 << 6;
    this->advanceCycles(8);
}

void CPU::set_7_b() {
    this->registers.bc.b.h |= 1 << 7;
    this->advanceCycles(8);
}

void CPU::set_7_c() {
    this->registers.bc.b.l |= 1 << 7;
    this->advanceCycles(8);
}

void CPU::set_7_d() {
    this->registers.de.b.h |= 1 << 7;
    this->advanceCycles(8);
}

void CPU::set_7_e() {
    this->registers.de.b.l |= 1 << 7;
    this->advanceCycles(8);
}

void CPU::set_7_h() {
    this->registers.hl.b.h |= 1 << 7;
    this->advanceCycles(8);
}

void CPU::set_7_l() {
    this->registers.hl.b.l |= 1 << 7;
    this->advanceCycles(8);
}

void CPU::set_7_hlp() {
    this->advanceCycles(4);
    u8 val = this->gameboy->mmu->read(this->registers.hl.w);
    this->advanceCycles(4);
    this->gameboy->mmu->write(this->registers.hl.w, val | (u8) (1 << 7));
    this->advanceCycles(8);
}

void CPU::set_7_a() {
    this->registers.af.b.h |= 1 << 7;
    this->advanceCycles(8);
}
