#include <string.h>

#include "platform/common/menu.h"
#include "platform/system.h"
#include "types.h"
#include "apu.h"
#include "cartridge.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "ppu.h"
#include "serial.h"
#include "sgb.h"
#include "timer.h"

static u8 temp1 = 0;
static u8 temp2 = 0;

#define FLAG_ZERO 0x80
#define FLAG_NEGATIVE 0x40
#define FLAG_HALFCARRY 0x20
#define FLAG_CARRY 0x10

#define FLAG_GET(f) ((this->registers.af.b.l & (f)) == (f))
#define FLAG_SET(f, x) (this->registers.af.b.l ^= (-(x) ^ this->registers.af.b.l) & (f))

#define SETPC(val) (this->registers.pc.w = (val), this->advanceCycles(4))

#define MEMREAD(addr) (temp1 = this->gameboy->mmu->read(addr), this->advanceCycles(4), temp1)
#define MEMWRITE(addr, val) (this->gameboy->mmu->write(addr, val), this->advanceCycles(4))

#define READPC8() MEMREAD(this->registers.pc.w++)
#define READPC16() (temp2 = READPC8(), temp2 | (READPC8() << 8))

#define PUSH8(val) MEMWRITE(--this->registers.sp.w, (val))
#define PUSH16(val) (PUSH8((u8) (((val) >> 8) & 0xFF)), PUSH8((u8) ((val) & 0xFF)))

#define POP8() MEMREAD(this->registers.sp.w++)
#define POP16() (temp2 = POP8(), temp2 | (POP8() << 8))

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

    this->gameboy->mmu->mapIOWriteFunc(IF, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(IF, (u8) (val | 0xE0));
    });

    this->gameboy->mmu->mapIOWriteFunc(KEY1, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            this->gameboy->mmu->writeIO(KEY1, (u8) ((this->gameboy->mmu->readIO(KEY1) & ~1) | (val & 1)));
        }
    });
}

void CPU::loadState(std::istream& data, u8 version) {
    data.read((char*) &this->cycleCount, sizeof(this->cycleCount));
    data.read((char*) &this->eventCycle, sizeof(this->eventCycle));
    data.read((char*) &this->registers, sizeof(this->registers));
    data.read((char*) &this->haltState, sizeof(this->haltState));
    data.read((char*) &this->haltBug, sizeof(this->haltBug));
    data.read((char*) &this->ime, sizeof(this->ime));
}

void CPU::saveState(std::ostream& data) {
    data.write((char*) &this->cycleCount, sizeof(this->cycleCount));
    data.write((char*) &this->eventCycle, sizeof(this->eventCycle));
    data.write((char*) &this->registers, sizeof(this->registers));
    data.write((char*) &this->haltState, sizeof(this->haltState));
    data.write((char*) &this->haltBug, sizeof(this->haltBug));
    data.write((char*) &this->ime, sizeof(this->ime));
}

void CPU::run() {
    if(!this->haltState) {
        u8 op = READPC8();

        if(this->haltBug) {
            this->registers.pc.w--;
            this->haltBug = false;
        }

        (this->*opcodes[op])();
    } else {
        this->advanceCycles(this->eventCycle - this->cycleCount);
    }

    int triggered = this->gameboy->mmu->readIO(IF) & this->gameboy->mmu->readIO(IE);
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

void CPU::advanceCycles(u64 cycles) {
    this->cycleCount += cycles;

    if(this->cycleCount >= this->eventCycle) {
        this->eventCycle = UINT64_MAX;

        if(this->gameboy->cartridge != NULL) {
            this->gameboy->cartridge->update();
        }

        this->gameboy->ppu->update();
        this->gameboy->apu->update();
        this->gameboy->sgb->update();
        this->gameboy->timer->update();
        this->gameboy->serial->update();
    }
}

void CPU::requestInterrupt(int id) {
    this->gameboy->mmu->writeIO(IF, (u8) (this->gameboy->mmu->readIO(IF) | id));
}

void CPU::undefined() {
    systemPrintDebug("Undefined instruction 0x%x at 0x%x.\n", this->gameboy->mmu->read((u16) (this->registers.pc.w - 1)), this->registers.pc.w - 1);
}

void CPU::nop() {
}

void CPU::ld_bc_nn() {
    this->registers.bc.w = READPC16();
}

void CPU::ld_bcp_a() {
    MEMWRITE(this->registers.bc.w, this->registers.af.b.h);
}

void CPU::inc_bc() {
    this->registers.bc.w++;
    this->advanceCycles(4);
}

void CPU::inc_b() {
    u8 result = (u8) (this->registers.bc.b.h + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.bc.b.h & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
}

void CPU::dec_b() {
    u8 result = (u8) (this->registers.bc.b.h - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.bc.b.h & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
}

void CPU::ld_b_n() {
    this->registers.bc.b.h = READPC8();
}

void CPU::rlca() {
    u8 carry = (u8) ((this->registers.af.b.h & 0x80) >> 7);
    u8 result = (this->registers.af.b.h << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.af.b.h = result;
}

void CPU::ld_nnp_sp() {
    u16 addr = READPC16();
    MEMWRITE(addr, (u8) (this->registers.sp.w & 0xFF));
    MEMWRITE((u16) (addr + 1), (u8) ((this->registers.sp.w >> 8) & 0xFF));
}

void CPU::add_hl_bc() {
    int result = this->registers.hl.w + this->registers.bc.w;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.w & 0xFFF) + (this->registers.bc.w & 0xFFF) >= 0x1000);
    FLAG_SET(FLAG_CARRY, result >= 0x10000);
    this->registers.hl.w = (u16) (result & 0xFFFF);
    this->advanceCycles(4);
}

void CPU::ld_a_bcp() {
    this->registers.af.b.h = MEMREAD(this->registers.bc.w);
}

void CPU::dec_bc() {
    this->registers.bc.w--;
    this->advanceCycles(4);
}

void CPU::inc_c() {
    u8 result = (u8) (this->registers.bc.b.l + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.bc.b.l & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
}

void CPU::dec_c() {
    u8 result = (u8) (this->registers.bc.b.l - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.bc.b.l & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
}

void CPU::ld_c_n() {
    this->registers.bc.b.l = READPC8();
}

void CPU::rrca() {
    u8 carry = (u8) ((this->registers.af.b.h & 0x01) << 7);
    u8 result = (this->registers.af.b.h >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.af.b.h = result;
}

void CPU::stop() {
    u8 key1 = this->gameboy->mmu->readIO(KEY1);
    if(this->gameboy->gbMode == MODE_CGB && (key1 & 0x01) != 0) {
        bool doubleSpeed = (key1 & 0x80) == 0;

        this->gameboy->apu->setHalfSpeed(doubleSpeed);
        this->gameboy->ppu->setHalfSpeed(doubleSpeed);

        this->gameboy->mmu->writeIO(KEY1, (u8) (key1 ^ 0x81));
        this->registers.pc.w++;
    } else {
        this->haltState = true;
    }
}

void CPU::ld_de_nn() {
    this->registers.de.w = READPC16();
}

void CPU::ld_dep_a() {
    MEMWRITE(this->registers.de.w, this->registers.af.b.h);
}

void CPU::inc_de() {
    this->registers.de.w++;
    this->advanceCycles(4);
}

void CPU::inc_d() {
    u8 result = (u8) (this->registers.de.b.h + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.de.b.h & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
}

void CPU::dec_d() {
    u8 result = (u8) (this->registers.de.b.h - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.de.b.h & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
}

void CPU::ld_d_n() {
    this->registers.de.b.h = READPC8();
}

void CPU::rla() {
    u8 result = (this->registers.af.b.h << 1) | (u8) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.af.b.h = result;
}

void CPU::jr_n() {
    s8 offset = READPC8();
    SETPC(this->registers.pc.w + offset);
}

void CPU::add_hl_de() {
    int result = this->registers.hl.w + this->registers.de.w;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.w & 0xFFF) + (this->registers.de.w & 0xFFF) >= 0x1000);
    FLAG_SET(FLAG_CARRY, result >= 0x10000);
    this->registers.hl.w = (u16) (result & 0xFFFF);
    this->advanceCycles(4);
}

void CPU::ld_a_dep() {
    this->registers.af.b.h = MEMREAD(this->registers.de.w);
}

void CPU::dec_de() {
    this->registers.de.w--;
    this->advanceCycles(4);
}

void CPU::inc_e() {
    u8 result = (u8) (this->registers.de.b.l + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.de.b.l & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
}

void CPU::dec_e() {
    u8 result = (u8) (this->registers.de.b.l - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.de.b.l & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
}

void CPU::ld_e_n() {
    this->registers.de.b.l = READPC8();
}

void CPU::rra() {
    u8 result = (this->registers.af.b.h >> 1) | (u8) (FLAG_GET(FLAG_CARRY) << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.af.b.h = result;
}

void CPU::jr_nz_n() {
    s8 offset = READPC8();
    if(!FLAG_GET(FLAG_ZERO)) {
        SETPC(this->registers.pc.w + offset);
    }
}

void CPU::ld_hl_nn() {
    this->registers.hl.w = READPC16();
}

void CPU::ldi_hlp_a() {
    MEMWRITE(this->registers.hl.w++, this->registers.af.b.h);
}

void CPU::inc_hl() {
    this->registers.hl.w++;
    this->advanceCycles(4);
}

void CPU::inc_h() {
    u8 result = (u8) (this->registers.hl.b.h + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.b.h & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
}

void CPU::dec_h() {
    u8 result = (u8) (this->registers.hl.b.h - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.b.h & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
}

void CPU::ld_h_n() {
    this->registers.hl.b.h = READPC8();
}

void CPU::daa() {
    int result = this->registers.af.b.h;
    if(FLAG_GET(FLAG_NEGATIVE)) {
        if(FLAG_GET(FLAG_HALFCARRY)) {
            result += 0xFA;
        }

        if(FLAG_GET(FLAG_CARRY)) {
            result += 0xA0;
        }
    } else {
        if(FLAG_GET(FLAG_HALFCARRY) || (result & 0xF) > 9) {
            result += 0x06;
        }

        if(FLAG_GET(FLAG_CARRY) || (result & 0x1F0) > 0x90) {
            result += 0x60;
            FLAG_SET(FLAG_CARRY, 1);
        } else {
            FLAG_SET(FLAG_CARRY, 0);
        }
    }

    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::jr_z_n() {
    s8 offset = READPC8();
    if(FLAG_GET(FLAG_ZERO)) {
        SETPC(this->registers.pc.w + offset);
    }
}

void CPU::add_hl_hl() {
    int result = this->registers.hl.w + this->registers.hl.w;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.w & 0xFFF) + (this->registers.hl.w & 0xFFF) >= 0x1000);
    FLAG_SET(FLAG_CARRY, result >= 0x10000);
    this->registers.hl.w = (u16) (result & 0xFFFF);
    this->advanceCycles(4);
}

void CPU::ldi_a_hlp() {
    this->registers.af.b.h = MEMREAD(this->registers.hl.w++);
}

void CPU::dec_hl() {
    this->registers.hl.w--;
    this->advanceCycles(4);
}

void CPU::inc_l() {
    u8 result = (u8) (this->registers.hl.b.l + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.b.l & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
}

void CPU::dec_l() {
    u8 result = (u8) (this->registers.hl.b.l - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.b.l & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
}

void CPU::ld_l_n() {
    this->registers.hl.b.l = READPC8();
}

void CPU::cpl() {
    this->registers.af.b.h = ~this->registers.af.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, 1);
}

void CPU::jr_nc_n() {
    s8 offset = READPC8();
    if(!FLAG_GET(FLAG_CARRY)) {
        SETPC(this->registers.pc.w + offset);
    }
}

void CPU::ld_sp_nn() {
    this->registers.sp.w = READPC16();
}

void CPU::ldd_hlp_a() {
    MEMWRITE(this->registers.hl.w--, this->registers.af.b.h);
}

void CPU::inc_sp() {
    this->registers.sp.w++;
    this->advanceCycles(4);
}

void CPU::inc_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 result = (u8) (val + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (val & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    MEMWRITE(this->registers.hl.w, result);
}

void CPU::dec_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 result = (u8) (val - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (val & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    MEMWRITE(this->registers.hl.w, result);
}

void CPU::ld_hlp_n() {
    u8 val = READPC8();
    MEMWRITE(this->registers.hl.w, val);
}

void CPU::scf() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 1);
}

void CPU::jr_c_n() {
    s8 offset = READPC8();
    if(FLAG_GET(FLAG_CARRY)) {
        SETPC(this->registers.pc.w + offset);
    }
}

void CPU::add_hl_sp() {
    int result = this->registers.hl.w + this->registers.sp.w;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.hl.w & 0xFFF) + (this->registers.sp.w & 0xFFF) >= 0x1000);
    FLAG_SET(FLAG_CARRY, result >= 0x10000);
    this->registers.hl.w = (u16) (result & 0xFFFF);
    this->advanceCycles(4);
}

void CPU::ldd_a_hlp() {
    this->registers.af.b.h = MEMREAD(this->registers.hl.w--);
}

void CPU::dec_sp() {
    this->registers.sp.w--;
    this->advanceCycles(4);
}

void CPU::inc_a() {
    u8 result = (u8) (this->registers.af.b.h + 1);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) == 0xF);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::dec_a() {
    u8 result = (u8) (this->registers.af.b.h - 1);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) == 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::ld_a_n() {
    this->registers.af.b.h = READPC8();
}

void CPU::ccf() {
    FLAG_SET(FLAG_CARRY, !FLAG_GET(FLAG_CARRY));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
}

void CPU::ld_b_c() {
    this->registers.bc.b.h = this->registers.bc.b.l;
}

void CPU::ld_b_d() {
    this->registers.bc.b.h = this->registers.de.b.h;
}

void CPU::ld_b_e() {
    this->registers.bc.b.h = this->registers.de.b.l;
}

void CPU::ld_b_h() {
    this->registers.bc.b.h = this->registers.hl.b.h;
}

void CPU::ld_b_l() {
    this->registers.bc.b.h = this->registers.hl.b.l;
}

void CPU::ld_b_hlp() {
    this->registers.bc.b.h = MEMREAD(this->registers.hl.w);
}

void CPU::ld_b_a() {
    this->registers.bc.b.h = this->registers.af.b.h;
}

void CPU::ld_c_b() {
    this->registers.bc.b.l = this->registers.bc.b.h;
}

void CPU::ld_c_d() {
    this->registers.bc.b.l = this->registers.de.b.h;
}

void CPU::ld_c_e() {
    this->registers.bc.b.l = this->registers.de.b.l;
}

void CPU::ld_c_h() {
    this->registers.bc.b.l = this->registers.hl.b.h;
}

void CPU::ld_c_l() {
    this->registers.bc.b.l = this->registers.hl.b.l;
}

void CPU::ld_c_hlp() {
    this->registers.bc.b.l = MEMREAD(this->registers.hl.w);
}

void CPU::ld_c_a() {
    this->registers.bc.b.l = this->registers.af.b.h;
}

void CPU::ld_d_b() {
    this->registers.de.b.h = this->registers.bc.b.h;
}

void CPU::ld_d_c() {
    this->registers.de.b.h = this->registers.bc.b.l;
}

void CPU::ld_d_e() {
    this->registers.de.b.h = this->registers.de.b.l;
}

void CPU::ld_d_h() {
    this->registers.de.b.h = this->registers.hl.b.h;
}

void CPU::ld_d_l() {
    this->registers.de.b.h = this->registers.hl.b.l;
}

void CPU::ld_d_hlp() {
    this->registers.de.b.h = MEMREAD(this->registers.hl.w);
}

void CPU::ld_d_a() {
    this->registers.de.b.h = this->registers.af.b.h;
}

void CPU::ld_e_b() {
    this->registers.de.b.l = this->registers.bc.b.h;
}

void CPU::ld_e_c() {
    this->registers.de.b.l = this->registers.bc.b.l;
}

void CPU::ld_e_d() {
    this->registers.de.b.l = this->registers.de.b.h;
}

void CPU::ld_e_h() {
    this->registers.de.b.l = this->registers.hl.b.h;
}

void CPU::ld_e_l() {
    this->registers.de.b.l = this->registers.hl.b.l;
}

void CPU::ld_e_hlp() {
    this->registers.de.b.l = MEMREAD(this->registers.hl.w);
}

void CPU::ld_e_a() {
    this->registers.de.b.l = this->registers.af.b.h;
}

void CPU::ld_h_b() {
    this->registers.hl.b.h = this->registers.bc.b.h;
}

void CPU::ld_h_c() {
    this->registers.hl.b.h = this->registers.bc.b.l;
}

void CPU::ld_h_d() {
    this->registers.hl.b.h = this->registers.de.b.h;
}

void CPU::ld_h_e() {
    this->registers.hl.b.h = this->registers.de.b.l;
}

void CPU::ld_h_l() {
    this->registers.hl.b.h = this->registers.hl.b.l;
}

void CPU::ld_h_hlp() {
    this->registers.hl.b.h = MEMREAD(this->registers.hl.w);
}

void CPU::ld_h_a() {
    this->registers.hl.b.h = this->registers.af.b.h;
}

void CPU::ld_l_b() {
    this->registers.hl.b.l = this->registers.bc.b.h;
}

void CPU::ld_l_c() {
    this->registers.hl.b.l = this->registers.bc.b.l;
}

void CPU::ld_l_d() {
    this->registers.hl.b.l = this->registers.de.b.h;
}

void CPU::ld_l_e() {
    this->registers.hl.b.l = this->registers.de.b.l;
}

void CPU::ld_l_h() {
    this->registers.hl.b.l = this->registers.hl.b.h;
}

void CPU::ld_l_hlp() {
    this->registers.hl.b.l = MEMREAD(this->registers.hl.w);
}

void CPU::ld_l_a() {
    this->registers.hl.b.l = this->registers.af.b.h;
}

void CPU::ld_hlp_b() {
    MEMWRITE(this->registers.hl.w, this->registers.bc.b.h);
}

void CPU::ld_hlp_c() {
    MEMWRITE(this->registers.hl.w, this->registers.bc.b.l);
}

void CPU::ld_hlp_d() {
    MEMWRITE(this->registers.hl.w, this->registers.de.b.h);
}

void CPU::ld_hlp_e() {
    MEMWRITE(this->registers.hl.w, this->registers.de.b.l);
}

void CPU::ld_hlp_h() {
    MEMWRITE(this->registers.hl.w, this->registers.hl.b.h);
}

void CPU::ld_hlp_l() {
    MEMWRITE(this->registers.hl.w, this->registers.hl.b.l);
}

void CPU::halt() {
    if(!this->ime && (this->gameboy->mmu->readIO(IF) & this->gameboy->mmu->readIO(IE) & 0x1F) != 0) {
        if(this->gameboy->gbMode != MODE_CGB) {
            this->haltBug = true;
        }
    } else {
        this->haltState = true;
    }
}

void CPU::ld_hlp_a() {
    MEMWRITE(this->registers.hl.w, this->registers.af.b.h);
}

void CPU::ld_a_b() {
    this->registers.af.b.h = this->registers.bc.b.h;
}

void CPU::ld_a_c() {
    this->registers.af.b.h = this->registers.bc.b.l;
}

void CPU::ld_a_d() {
    this->registers.af.b.h = this->registers.de.b.h;
}

void CPU::ld_a_e() {
    this->registers.af.b.h = this->registers.de.b.l;
}

void CPU::ld_a_h() {
    this->registers.af.b.h = this->registers.hl.b.h;
}

void CPU::ld_a_l() {
    this->registers.af.b.h = this->registers.hl.b.l;
}

void CPU::ld_a_hlp() {
    this->registers.af.b.h = MEMREAD(this->registers.hl.w);
}

void CPU::add_a_b() {
    u16 result = this->registers.af.b.h + this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.bc.b.h & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::add_a_c() {
    u16 result = this->registers.af.b.h + this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.bc.b.l & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::add_a_d() {
    u16 result = this->registers.af.b.h + this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.de.b.h & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::add_a_e() {
    u16 result = this->registers.af.b.h + this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.de.b.l & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::add_a_h() {
    u16 result = this->registers.af.b.h + this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.hl.b.h & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::add_a_l() {
    u16 result = this->registers.af.b.h + this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.hl.b.l & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::add_a_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u16 result = this->registers.af.b.h + val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (val & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::add_a_a() {
    u16 result = this->registers.af.b.h + this->registers.af.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.af.b.h & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::adc_b() {
    u16 result = this->registers.af.b.h + this->registers.bc.b.h + (u16) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.bc.b.h & 0xF) + FLAG_GET(FLAG_CARRY) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::adc_c() {
    u16 result = this->registers.af.b.h + this->registers.bc.b.l + (u16) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.bc.b.l & 0xF) + FLAG_GET(FLAG_CARRY) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::adc_d() {
    u16 result = this->registers.af.b.h + this->registers.de.b.h + (u16) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.de.b.h & 0xF) + FLAG_GET(FLAG_CARRY) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::adc_e() {
    u16 result = this->registers.af.b.h + this->registers.de.b.l + (u16) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.de.b.l & 0xF) + FLAG_GET(FLAG_CARRY) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::adc_h() {
    u16 result = this->registers.af.b.h + this->registers.hl.b.h + (u16) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.hl.b.h & 0xF) + FLAG_GET(FLAG_CARRY) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::adc_l() {
    u16 result = this->registers.af.b.h + this->registers.hl.b.l + (u16) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.hl.b.l & 0xF) + FLAG_GET(FLAG_CARRY) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::adc_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u16 result = this->registers.af.b.h + val + (u16) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (val & 0xF) + FLAG_GET(FLAG_CARRY) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::adc_a() {
    u16 result = this->registers.af.b.h + this->registers.af.b.h + (u16) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (this->registers.af.b.h & 0xF) + FLAG_GET(FLAG_CARRY) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sub_b() {
    int result = this->registers.af.b.h - this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sub_c() {
    int result = this->registers.af.b.h - this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sub_d() {
    int result = this->registers.af.b.h - this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sub_e() {
    int result = this->registers.af.b.h - this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sub_h() {
    int result = this->registers.af.b.h - this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sub_l() {
    int result = this->registers.af.b.h - this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sub_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    int result = this->registers.af.b.h - val;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sub_a() {
    int result = this->registers.af.b.h - this->registers.af.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.af.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sbc_b() {
    int result = this->registers.af.b.h - this->registers.bc.b.h - FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.h & 0xF) - FLAG_GET(FLAG_CARRY) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sbc_c() {
    int result = this->registers.af.b.h - this->registers.bc.b.l - FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.l & 0xF) - FLAG_GET(FLAG_CARRY) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sbc_d() {
    int result = this->registers.af.b.h - this->registers.de.b.h - FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.h & 0xF) - FLAG_GET(FLAG_CARRY) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sbc_e() {
    int result = this->registers.af.b.h - this->registers.de.b.l - FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.l & 0xF) - FLAG_GET(FLAG_CARRY) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sbc_h() {
    int result = this->registers.af.b.h - this->registers.hl.b.h - FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.h & 0xF) - FLAG_GET(FLAG_CARRY) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sbc_l() {
    int result = this->registers.af.b.h - this->registers.hl.b.l - FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.l & 0xF) - FLAG_GET(FLAG_CARRY) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sbc_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    int result = this->registers.af.b.h - val - FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) - FLAG_GET(FLAG_CARRY) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::sbc_a() {
    int result = this->registers.af.b.h - this->registers.af.b.h - FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.af.b.h & 0xF) - FLAG_GET(FLAG_CARRY) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::and_b() {
    u8 result = this->registers.af.b.h & this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::and_c() {
    u8 result = this->registers.af.b.h & this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::and_d() {
    u8 result = this->registers.af.b.h & this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::and_e() {
    u8 result = this->registers.af.b.h & this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::and_h() {
    u8 result = this->registers.af.b.h & this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::and_l() {
    u8 result = this->registers.af.b.h & this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::and_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 result = this->registers.af.b.h & val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::and_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, this->registers.af.b.h == 0);
}

void CPU::xor_b() {
    u8 result = this->registers.af.b.h ^ this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::xor_c() {
    u8 result = this->registers.af.b.h ^ this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::xor_d() {
    u8 result = this->registers.af.b.h ^ this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::xor_e() {
    u8 result = this->registers.af.b.h ^ this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::xor_h() {
    u8 result = this->registers.af.b.h ^ this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::xor_l() {
    u8 result = this->registers.af.b.h ^ this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::xor_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 result = this->registers.af.b.h ^ val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::xor_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, 1);
    this->registers.af.b.h = 0;
}

void CPU::or_b() {
    u8 result = this->registers.af.b.h | this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::or_c() {
    u8 result = this->registers.af.b.h | this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::or_d() {
    u8 result = this->registers.af.b.h | this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::or_e() {
    u8 result = this->registers.af.b.h | this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::or_h() {
    u8 result = this->registers.af.b.h | this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::or_l() {
    u8 result = this->registers.af.b.h | this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::or_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 result = this->registers.af.b.h | val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::or_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, this->registers.af.b.h == 0);
}

void CPU::cp_b() {
    int result = this->registers.af.b.h - this->registers.bc.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
}

void CPU::cp_c() {
    int result = this->registers.af.b.h - this->registers.bc.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.bc.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
}

void CPU::cp_d() {
    int result = this->registers.af.b.h - this->registers.de.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
}

void CPU::cp_e() {
    int result = this->registers.af.b.h - this->registers.de.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.de.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
}

void CPU::cp_h() {
    int result = this->registers.af.b.h - this->registers.hl.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
}

void CPU::cp_l() {
    int result = this->registers.af.b.h - this->registers.hl.b.l;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.hl.b.l & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
}

void CPU::cp_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    int result = this->registers.af.b.h - val;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
}

void CPU::cp_a() {
    int result = this->registers.af.b.h - this->registers.af.b.h;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (this->registers.af.b.h & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
}

void CPU::ret_nz() {
    this->advanceCycles(4);
    if(!FLAG_GET(FLAG_ZERO)) {
        u16 addr = POP16();
        SETPC(addr);
    }
}

void CPU::pop_bc() {
    this->registers.bc.w = POP16();
}

void CPU::jp_nz_nn() {
    u16 addr = READPC16();
    if(!FLAG_GET(FLAG_ZERO)) {
        SETPC(addr);
    }
}

void CPU::jp_nn() {
    u16 addr = READPC16();
    SETPC(addr);
}

void CPU::call_nz_nn() {
    u16 addr = READPC16();
    if(!FLAG_GET(FLAG_ZERO)) {
        PUSH16(this->registers.pc.w);
        SETPC(addr);
    }
}

void CPU::push_bc() {
    PUSH16(this->registers.bc.w);
    this->advanceCycles(4);
}

void CPU::add_a_n() {
    u8 val = READPC8();
    u16 result = this->registers.af.b.h + val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (val & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::rst_0() {
    PUSH16(this->registers.pc.w);
    SETPC(0x0000);
}

void CPU::ret_z() {
    this->advanceCycles(4);
    if(FLAG_GET(FLAG_ZERO)) {
        u16 addr = POP16();
        SETPC(addr);
    }
}

void CPU::ret() {
    u16 addr = POP16();
    SETPC(addr);
}

void CPU::jp_z_nn() {
    u16 addr = READPC16();
    if(FLAG_GET(FLAG_ZERO)) {
        SETPC(addr);
    }
}

void CPU::cb_n() {
    u8 op = READPC8();
    (this->*cbOpcodes[op])();
}

void CPU::call_z_nn() {
    u16 addr = READPC16();
    if(FLAG_GET(FLAG_ZERO)) {
        PUSH16(this->registers.pc.w);
        SETPC(addr);
    }
}

void CPU::call_nn() {
    u16 addr = READPC16();
    PUSH16(this->registers.pc.w);
    SETPC(addr);
}

void CPU::adc_n() {
    u8 val = READPC8();
    u16 result = this->registers.af.b.h + val + (u16) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) + (val & 0xF) + FLAG_GET(FLAG_CARRY) >= 0x10);
    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::rst_08() {
    PUSH16(this->registers.pc.w);
    SETPC(0x0008);
}

void CPU::ret_nc() {
    this->advanceCycles(4);
    if(!FLAG_GET(FLAG_CARRY)) {
        u16 addr = POP16();
        SETPC(addr);
    }
}

void CPU::pop_de() {
    this->registers.de.w = POP16();
}

void CPU::jp_nc_nn() {
    u16 addr = READPC16();
    if(!FLAG_GET(FLAG_CARRY)) {
        SETPC(addr);
    }
}

void CPU::call_nc_nn() {
    u16 addr = READPC16();
    if(!FLAG_GET(FLAG_CARRY)) {
        PUSH16(this->registers.pc.w);
        SETPC(addr);
    }
}

void CPU::push_de() {
    PUSH16(this->registers.de.w);
    this->advanceCycles(4);
}

void CPU::sub_n() {
    u8 val = READPC8();
    int result = this->registers.af.b.h - val;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::rst_10() {
    PUSH16(this->registers.pc.w);
    SETPC(0x0010);
}

void CPU::ret_c() {
    this->advanceCycles(4);
    if(FLAG_GET(FLAG_CARRY)) {
        u16 addr = POP16();
        SETPC(addr);
    }
}

void CPU::reti() {
    u16 addr = POP16();
    this->ime = true;
    SETPC(addr);
}

void CPU::jp_c_nn() {
    u16 addr = READPC16();
    if(FLAG_GET(FLAG_CARRY)) {
        SETPC(addr);
    }
}

void CPU::call_c_nn() {
    u16 addr = READPC16();
    if(FLAG_GET(FLAG_CARRY)) {
        PUSH16(this->registers.pc.w);
        SETPC(addr);
    }
}

void CPU::sbc_n() {
    u8 val = READPC8();
    int result = this->registers.af.b.h - val - FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) - FLAG_GET(FLAG_CARRY) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
    this->registers.af.b.h = (u8) (result & 0xFF);
}

void CPU::rst_18() {
    PUSH16(this->registers.pc.w);
    SETPC(0x0018);
}

void CPU::ld_ff_n_a() {
    u8 reg = READPC8();
    MEMWRITE((u16) (0xFF00 + reg), this->registers.af.b.h);
}

void CPU::pop_hl() {
    this->registers.hl.w = POP16();
}

void CPU::ld_ff_c_a() {
    MEMWRITE((u16) (0xFF00 + this->registers.bc.b.l), this->registers.af.b.h);
}

void CPU::push_hl() {
    PUSH16(this->registers.hl.w);
    this->advanceCycles(4);
}

void CPU::and_n() {
    u8 val = READPC8();
    u8 result = this->registers.af.b.h & val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::rst_20() {
    PUSH16(this->registers.pc.w);
    SETPC(0x0020);
}

void CPU::add_sp_n() {
    u8 val = READPC8();
    u16 result = this->registers.sp.w + (s8) val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.sp.w & 0xF) + (val & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, (this->registers.sp.w & 0xFF) + val >= 0x100);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.sp.w = result;
    this->advanceCycles(8);
}

void CPU::jp_hl() {
    this->registers.pc.w = this->registers.hl.w;
}

void CPU::ld_nnp_a() {
    u16 addr = READPC16();
    MEMWRITE(addr, this->registers.af.b.h);
}

void CPU::xor_n() {
    u8 val = READPC8();
    u8 result = this->registers.af.b.h ^ val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::rst_28() {
    PUSH16(this->registers.pc.w);
    SETPC(0x0028);
}

void CPU::ld_a_ff_n() {
    u8 reg = READPC8();
    this->registers.af.b.h = MEMREAD((u16) (0xFF00 + reg));
}

void CPU::pop_af() {
    u8 oldFLow = (u8) (this->registers.af.b.l & 0xF);
    this->registers.af.w = POP16();
    this->registers.af.b.l = (u8) ((this->registers.af.b.l & 0xF0) | oldFLow);
}

void CPU::ld_a_ff_c() {
    this->registers.af.b.h = MEMREAD((u16) (0xFF00 + this->registers.bc.b.l));
}

void CPU::di_inst() {
    this->ime = false;
}

void CPU::push_af() {
    PUSH16(this->registers.af.w);
    this->advanceCycles(4);
}

void CPU::or_n() {
    u8 val = READPC8();
    u8 result = this->registers.af.b.h | val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::rst_30() {
    PUSH16(this->registers.pc.w);
    SETPC(0x0030);
}

void CPU::ld_hl_sp_n() {
    u8 val = READPC8();
    u16 result = this->registers.sp.w + (s8) val;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.sp.w & 0xF) + (val & 0xF) >= 0x10);
    FLAG_SET(FLAG_CARRY, (this->registers.sp.w & 0xFF) + val >= 0x100);
    FLAG_SET(FLAG_ZERO, 0);
    this->registers.hl.w = result;
    this->advanceCycles(4);
}

void CPU::ld_sp_hl() {
    this->registers.sp.w = this->registers.hl.w;
    this->advanceCycles(4);
}

void CPU::ld_a_nnp() {
    u16 addr = READPC16();
    this->registers.af.b.h = MEMREAD(addr);
}

void CPU::ei() {
    this->ime = true;
}

void CPU::cp_n() {
    u8 val = READPC8();
    int result = this->registers.af.b.h - val;
    FLAG_SET(FLAG_NEGATIVE, 1);
    FLAG_SET(FLAG_HALFCARRY, (this->registers.af.b.h & 0xF) - (val & 0xF) < 0);
    FLAG_SET(FLAG_CARRY, result < 0);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);
}

void CPU::rst_38() {
    PUSH16(this->registers.pc.w);
    SETPC(0x0038);
}

void CPU::rlc_b() {
    u8 carry = (u8) ((this->registers.bc.b.h & 0x80) >> 7);
    u8 result = (this->registers.bc.b.h << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
}

void CPU::rlc_c() {
    u8 carry = (u8) ((this->registers.bc.b.l & 0x80) >> 7);
    u8 result = (this->registers.bc.b.l << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
}

void CPU::rlc_d() {
    u8 carry = (u8) ((this->registers.de.b.h & 0x80) >> 7);
    u8 result = (this->registers.de.b.h << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
}

void CPU::rlc_e() {
    u8 carry = (u8) ((this->registers.de.b.l & 0x80) >> 7);
    u8 result = (this->registers.de.b.l << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
}

void CPU::rlc_h() {
    u8 carry = (u8) ((this->registers.hl.b.h & 0x80) >> 7);
    u8 result = (this->registers.hl.b.h << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
}

void CPU::rlc_l() {
    u8 carry = (u8) ((this->registers.hl.b.l & 0x80) >> 7);
    u8 result = (this->registers.hl.b.l << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
}

void CPU::rlc_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 carry = (u8) ((val & 0x80) >> 7);
    u8 result = (val << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    MEMWRITE(this->registers.hl.w, result);
}

void CPU::rlc_a() {
    u8 carry = (u8) ((this->registers.af.b.h & 0x80) >> 7);
    u8 result = (this->registers.af.b.h << 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::rrc_b() {
    u8 carry = (u8) ((this->registers.bc.b.h & 0x01) << 7);
    u8 result = (this->registers.bc.b.h >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
}

void CPU::rrc_c() {
    u8 carry = (u8) ((this->registers.bc.b.l & 0x01) << 7);
    u8 result = (this->registers.bc.b.l >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
}

void CPU::rrc_d() {
    u8 carry = (u8) ((this->registers.de.b.h & 0x01) << 7);
    u8 result = (this->registers.de.b.h >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
}

void CPU::rrc_e() {
    u8 carry = (u8) ((this->registers.de.b.l & 0x01) << 7);
    u8 result = (this->registers.de.b.l >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
}

void CPU::rrc_h() {
    u8 carry = (u8) ((this->registers.hl.b.h & 0x01) << 7);
    u8 result = (this->registers.hl.b.h >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
}

void CPU::rrc_l() {
    u8 carry = (u8) ((this->registers.hl.b.l & 0x01) << 7);
    u8 result = (this->registers.hl.b.l >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
}

void CPU::rrc_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 carry = (u8) ((val & 0x01) << 7);
    u8 result = (val >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    MEMWRITE(this->registers.hl.w, result);
}

void CPU::rrc_a() {
    u8 carry = (u8) ((this->registers.af.b.h & 0x01) << 7);
    u8 result = (this->registers.af.b.h >> 1) | carry;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::rl_b() {
    u8 result = (this->registers.bc.b.h << 1) | (u8) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
}

void CPU::rl_c() {
    u8 result = (this->registers.bc.b.l << 1) | (u8) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
}

void CPU::rl_d() {
    u8 result = (this->registers.de.b.h << 1) | (u8) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
}

void CPU::rl_e() {
    u8 result = (this->registers.de.b.l << 1) | (u8) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
}

void CPU::rl_h() {
    u8 result = (this->registers.hl.b.h << 1) | (u8) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
}

void CPU::rl_l() {
    u8 result = (this->registers.hl.b.l << 1) | (u8) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
}

void CPU::rl_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 result = (val << 1) | (u8) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (val & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    MEMWRITE(this->registers.hl.w, result);
}

void CPU::rl_a() {
    u8 result = (this->registers.af.b.h << 1) | (u8) FLAG_GET(FLAG_CARRY);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::rr_b() {
    u8 result = (this->registers.bc.b.h >> 1) | (u8) (FLAG_GET(FLAG_CARRY) << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
}

void CPU::rr_c() {
    u8 result = (this->registers.bc.b.l >> 1) | (u8) (FLAG_GET(FLAG_CARRY) << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
}

void CPU::rr_d() {
    u8 result = (this->registers.de.b.h >> 1) | (u8) (FLAG_GET(FLAG_CARRY) << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
}

void CPU::rr_e() {
    u8 result = (this->registers.de.b.l >> 1) | (u8) (FLAG_GET(FLAG_CARRY) << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
}

void CPU::rr_h() {
    u8 result = (this->registers.hl.b.h >> 1) | (u8) (FLAG_GET(FLAG_CARRY) << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
}

void CPU::rr_l() {
    u8 result = (this->registers.hl.b.l >> 1) | (u8) (FLAG_GET(FLAG_CARRY) << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
}

void CPU::rr_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 result = (val >> 1) | (u8) (FLAG_GET(FLAG_CARRY) << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (val & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    MEMWRITE(this->registers.hl.w, result);
}

void CPU::rr_a() {
    u8 result = (this->registers.af.b.h >> 1) | (u8) (FLAG_GET(FLAG_CARRY) << 7);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::sla_b() {
    u8 result = this->registers.bc.b.h << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
}

void CPU::sla_c() {
    u8 result = this->registers.bc.b.l << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
}

void CPU::sla_d() {
    u8 result = this->registers.de.b.h << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
}

void CPU::sla_e() {
    u8 result = this->registers.de.b.l << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
}

void CPU::sla_h() {
    u8 result = this->registers.hl.b.h << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
}

void CPU::sla_l() {
    u8 result = this->registers.hl.b.l << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.l & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
}

void CPU::sla_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 result = val << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (val & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    MEMWRITE(this->registers.hl.w, result);
}

void CPU::sla_a() {
    u8 result = this->registers.af.b.h << 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x80) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::sra_b() {
    u8 result = (this->registers.bc.b.h >> 1) | (u8) (this->registers.bc.b.h & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
}

void CPU::sra_c() {
    u8 result = (this->registers.bc.b.l >> 1) | (u8) (this->registers.bc.b.l & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
}

void CPU::sra_d() {
    u8 result = (this->registers.de.b.h >> 1) | (u8) (this->registers.de.b.h & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
}

void CPU::sra_e() {
    u8 result = (this->registers.de.b.l >> 1) | (u8) (this->registers.de.b.l & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
}

void CPU::sra_h() {
    u8 result = (this->registers.hl.b.h >> 1) | (u8) (this->registers.hl.b.h & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
}

void CPU::sra_l() {
    u8 result = (this->registers.hl.b.l >> 1) | (u8) (this->registers.hl.b.l & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
}

void CPU::sra_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 result = (val >> 1) | (u8) (val & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (val & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    MEMWRITE(this->registers.hl.w, result);
}

void CPU::sra_a() {
    u8 result = (this->registers.af.b.h >> 1) | (u8) (this->registers.af.b.h & 0x80);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::swap_b() {
    u8 result = (u8) (((this->registers.bc.b.h & 0x0F) << 4) | ((this->registers.bc.b.h & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
}

void CPU::swap_c() {
    u8 result = (u8) (((this->registers.bc.b.l & 0x0F) << 4) | ((this->registers.bc.b.l & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
}

void CPU::swap_d() {
    u8 result = (u8) (((this->registers.de.b.h & 0x0F) << 4) | ((this->registers.de.b.h & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
}

void CPU::swap_e() {
    u8 result = (u8) (((this->registers.de.b.l & 0x0F) << 4) | ((this->registers.de.b.l & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
}

void CPU::swap_h() {
    u8 result = (u8) (((this->registers.hl.b.h & 0x0F) << 4) | ((this->registers.hl.b.h & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
}

void CPU::swap_l() {
    u8 result = (u8) (((this->registers.hl.b.l & 0x0F) << 4) | ((this->registers.hl.b.l & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
}

void CPU::swap_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 result = (u8) (((val & 0x0F) << 4) | ((val & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    MEMWRITE(this->registers.hl.w, result);
}

void CPU::swap_a() {
    u8 result = (u8) (((this->registers.af.b.h & 0x0F) << 4) | ((this->registers.af.b.h & 0xF0) >> 4));
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::srl_b() {
    u8 result = this->registers.bc.b.h >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.h = result;
}

void CPU::srl_c() {
    u8 result = this->registers.bc.b.l >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.bc.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.bc.b.l = result;
}

void CPU::srl_d() {
    u8 result = this->registers.de.b.h >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.h = result;
}

void CPU::srl_e() {
    u8 result = this->registers.de.b.l >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.de.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.de.b.l = result;
}

void CPU::srl_h() {
    u8 result = this->registers.hl.b.h >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.h = result;
}

void CPU::srl_l() {
    u8 result = this->registers.hl.b.l >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.hl.b.l & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.hl.b.l = result;
}

void CPU::srl_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    u8 result = val >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (val & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    MEMWRITE(this->registers.hl.w, result);
}

void CPU::srl_a() {
    u8 result = this->registers.af.b.h >> 1;
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, (this->registers.af.b.h & 0x01) != 0);
    FLAG_SET(FLAG_ZERO, result == 0);
    this->registers.af.b.h = result;
}

void CPU::bit_0_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 0)) == 0);
}

void CPU::bit_0_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 0)) == 0);
}

void CPU::bit_0_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 0)) == 0);
}

void CPU::bit_0_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 0)) == 0);
}

void CPU::bit_0_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 0)) == 0);
}

void CPU::bit_0_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 0)) == 0);
}

void CPU::bit_0_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (val & (1 << 0)) == 0);
}

void CPU::bit_0_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 0)) == 0);
}

void CPU::bit_1_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 1)) == 0);
}

void CPU::bit_1_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 1)) == 0);
}

void CPU::bit_1_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 1)) == 0);
}

void CPU::bit_1_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 1)) == 0);
}

void CPU::bit_1_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 1)) == 0);
}

void CPU::bit_1_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 1)) == 0);
}

void CPU::bit_1_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (val & (1 << 1)) == 0);
}

void CPU::bit_1_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 1)) == 0);
}

void CPU::bit_2_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 2)) == 0);
}

void CPU::bit_2_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 2)) == 0);
}

void CPU::bit_2_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 2)) == 0);
}

void CPU::bit_2_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 2)) == 0);
}

void CPU::bit_2_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 2)) == 0);
}

void CPU::bit_2_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 2)) == 0);
}

void CPU::bit_2_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (val & (1 << 2)) == 0);
}

void CPU::bit_2_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 2)) == 0);
}

void CPU::bit_3_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 3)) == 0);
}

void CPU::bit_3_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 3)) == 0);
}

void CPU::bit_3_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 3)) == 0);
}

void CPU::bit_3_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 3)) == 0);
}

void CPU::bit_3_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 3)) == 0);
}

void CPU::bit_3_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 3)) == 0);
}

void CPU::bit_3_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (val & (1 << 3)) == 0);
}

void CPU::bit_3_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 3)) == 0);
}

void CPU::bit_4_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 4)) == 0);
}

void CPU::bit_4_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 4)) == 0);
}

void CPU::bit_4_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 4)) == 0);
}

void CPU::bit_4_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 4)) == 0);
}

void CPU::bit_4_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 4)) == 0);
}

void CPU::bit_4_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 4)) == 0);
}

void CPU::bit_4_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (val & (1 << 4)) == 0);
}

void CPU::bit_4_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 4)) == 0);
}

void CPU::bit_5_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 5)) == 0);
}

void CPU::bit_5_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 5)) == 0);
}

void CPU::bit_5_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 5)) == 0);
}

void CPU::bit_5_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 5)) == 0);
}

void CPU::bit_5_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 5)) == 0);
}

void CPU::bit_5_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 5)) == 0);
}

void CPU::bit_5_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (val & (1 << 5)) == 0);
}

void CPU::bit_5_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 5)) == 0);
}

void CPU::bit_6_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 6)) == 0);
}

void CPU::bit_6_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 6)) == 0);
}

void CPU::bit_6_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 6)) == 0);
}

void CPU::bit_6_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 6)) == 0);
}

void CPU::bit_6_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 6)) == 0);
}

void CPU::bit_6_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 6)) == 0);
}

void CPU::bit_6_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (val & (1 << 6)) == 0);
}

void CPU::bit_6_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 6)) == 0);
}

void CPU::bit_7_b() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.h & (1 << 7)) == 0);
}

void CPU::bit_7_c() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.bc.b.l & (1 << 7)) == 0);
}

void CPU::bit_7_d() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.h & (1 << 7)) == 0);
}

void CPU::bit_7_e() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.de.b.l & (1 << 7)) == 0);
}

void CPU::bit_7_h() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.h & (1 << 7)) == 0);
}

void CPU::bit_7_l() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.hl.b.l & (1 << 7)) == 0);
}

void CPU::bit_7_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (val & (1 << 7)) == 0);
}

void CPU::bit_7_a() {
    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 1);
    FLAG_SET(FLAG_ZERO, (this->registers.af.b.h & (1 << 7)) == 0);
}

void CPU::res_0_b() {
    this->registers.bc.b.h &= ~(1 << 0);
}

void CPU::res_0_c() {
    this->registers.bc.b.l &= ~(1 << 0);
}

void CPU::res_0_d() {
    this->registers.de.b.h &= ~(1 << 0);
}

void CPU::res_0_e() {
    this->registers.de.b.l &= ~(1 << 0);
}

void CPU::res_0_h() {
    this->registers.hl.b.h &= ~(1 << 0);
}

void CPU::res_0_l() {
    this->registers.hl.b.l &= ~(1 << 0);
}

void CPU::res_0_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val & (u8) ~(1 << 0));
}

void CPU::res_0_a() {
    this->registers.af.b.h &= ~(1 << 0);
}

void CPU::res_1_b() {
    this->registers.bc.b.h &= ~(1 << 1);
}

void CPU::res_1_c() {
    this->registers.bc.b.l &= ~(1 << 1);
}

void CPU::res_1_d() {
    this->registers.de.b.h &= ~(1 << 1);
}

void CPU::res_1_e() {
    this->registers.de.b.l &= ~(1 << 1);
}

void CPU::res_1_h() {
    this->registers.hl.b.h &= ~(1 << 1);
}

void CPU::res_1_l() {
    this->registers.hl.b.l &= ~(1 << 1);
}

void CPU::res_1_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val & (u8) ~(1 << 1));
}

void CPU::res_1_a() {
    this->registers.af.b.h &= ~(1 << 1);
}

void CPU::res_2_b() {
    this->registers.bc.b.h &= ~(1 << 2);
}

void CPU::res_2_c() {
    this->registers.bc.b.l &= ~(1 << 2);
}

void CPU::res_2_d() {
    this->registers.de.b.h &= ~(1 << 2);
}

void CPU::res_2_e() {
    this->registers.de.b.l &= ~(1 << 2);
}

void CPU::res_2_h() {
    this->registers.hl.b.h &= ~(1 << 2);
}

void CPU::res_2_l() {
    this->registers.hl.b.l &= ~(1 << 2);
}

void CPU::res_2_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val & (u8) ~(1 << 2));
}

void CPU::res_2_a() {
    this->registers.af.b.h &= ~(1 << 2);
}

void CPU::res_3_b() {
    this->registers.bc.b.h &= ~(1 << 3);
}

void CPU::res_3_c() {
    this->registers.bc.b.l &= ~(1 << 3);
}

void CPU::res_3_d() {
    this->registers.de.b.h &= ~(1 << 3);
}

void CPU::res_3_e() {
    this->registers.de.b.l &= ~(1 << 3);
}

void CPU::res_3_h() {
    this->registers.hl.b.h &= ~(1 << 3);
}

void CPU::res_3_l() {
    this->registers.hl.b.l &= ~(1 << 3);
}

void CPU::res_3_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val & (u8) ~(1 << 3));
}

void CPU::res_3_a() {
    this->registers.af.b.h &= ~(1 << 3);
}

void CPU::res_4_b() {
    this->registers.bc.b.h &= ~(1 << 4);
}

void CPU::res_4_c() {
    this->registers.bc.b.l &= ~(1 << 4);
}

void CPU::res_4_d() {
    this->registers.de.b.h &= ~(1 << 4);
}

void CPU::res_4_e() {
    this->registers.de.b.l &= ~(1 << 4);
}

void CPU::res_4_h() {
    this->registers.hl.b.h &= ~(1 << 4);
}

void CPU::res_4_l() {
    this->registers.hl.b.l &= ~(1 << 4);
}

void CPU::res_4_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val & (u8) ~(1 << 4));
}

void CPU::res_4_a() {
    this->registers.af.b.h &= ~(1 << 4);
}

void CPU::res_5_b() {
    this->registers.bc.b.h &= ~(1 << 5);
}

void CPU::res_5_c() {
    this->registers.bc.b.l &= ~(1 << 5);
}

void CPU::res_5_d() {
    this->registers.de.b.h &= ~(1 << 5);
}

void CPU::res_5_e() {
    this->registers.de.b.l &= ~(1 << 5);
}

void CPU::res_5_h() {
    this->registers.hl.b.h &= ~(1 << 5);
}

void CPU::res_5_l() {
    this->registers.hl.b.l &= ~(1 << 5);
}

void CPU::res_5_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val & (u8) ~(1 << 5));
}

void CPU::res_5_a() {
    this->registers.af.b.h &= ~(1 << 5);
}

void CPU::res_6_b() {
    this->registers.bc.b.h &= ~(1 << 6);
}

void CPU::res_6_c() {
    this->registers.bc.b.l &= ~(1 << 6);
}

void CPU::res_6_d() {
    this->registers.de.b.h &= ~(1 << 6);
}

void CPU::res_6_e() {
    this->registers.de.b.l &= ~(1 << 6);
}

void CPU::res_6_h() {
    this->registers.hl.b.h &= ~(1 << 6);
}

void CPU::res_6_l() {
    this->registers.hl.b.l &= ~(1 << 6);
}

void CPU::res_6_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val & (u8) ~(1 << 6));
}

void CPU::res_6_a() {
    this->registers.af.b.h &= ~(1 << 6);
}

void CPU::res_7_b() {
    this->registers.bc.b.h &= ~(1 << 7);
}

void CPU::res_7_c() {
    this->registers.bc.b.l &= ~(1 << 7);
}

void CPU::res_7_d() {
    this->registers.de.b.h &= ~(1 << 7);
}

void CPU::res_7_e() {
    this->registers.de.b.l &= ~(1 << 7);
}

void CPU::res_7_h() {
    this->registers.hl.b.h &= ~(1 << 7);
}

void CPU::res_7_l() {
    this->registers.hl.b.l &= ~(1 << 7);
}

void CPU::res_7_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val & (u8) ~(1 << 7));
}

void CPU::res_7_a() {
    this->registers.af.b.h &= ~(1 << 7);
}

void CPU::set_0_b() {
    this->registers.bc.b.h |= 1 << 0;
}

void CPU::set_0_c() {
    this->registers.bc.b.l |= 1 << 0;
}

void CPU::set_0_d() {
    this->registers.de.b.h |= 1 << 0;
}

void CPU::set_0_e() {
    this->registers.de.b.l |= 1 << 0;
}

void CPU::set_0_h() {
    this->registers.hl.b.h |= 1 << 0;
}

void CPU::set_0_l() {
    this->registers.hl.b.l |= 1 << 0;
}

void CPU::set_0_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val | (u8) (1 << 0));
}

void CPU::set_0_a() {
    this->registers.af.b.h |= 1 << 0;
}

void CPU::set_1_b() {
    this->registers.bc.b.h |= 1 << 1;
}

void CPU::set_1_c() {
    this->registers.bc.b.l |= 1 << 1;
}

void CPU::set_1_d() {
    this->registers.de.b.h |= 1 << 1;
}

void CPU::set_1_e() {
    this->registers.de.b.l |= 1 << 1;
}

void CPU::set_1_h() {
    this->registers.hl.b.h |= 1 << 1;
}

void CPU::set_1_l() {
    this->registers.hl.b.l |= 1 << 1;
}

void CPU::set_1_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val | (u8) (1 << 1));
}

void CPU::set_1_a() {
    this->registers.af.b.h |= 1 << 1;
}

void CPU::set_2_b() {
    this->registers.bc.b.h |= 1 << 2;
}

void CPU::set_2_c() {
    this->registers.bc.b.l |= 1 << 2;
}

void CPU::set_2_d() {
    this->registers.de.b.h |= 1 << 2;
}

void CPU::set_2_e() {
    this->registers.de.b.l |= 1 << 2;
}

void CPU::set_2_h() {
    this->registers.hl.b.h |= 1 << 2;
}

void CPU::set_2_l() {
    this->registers.hl.b.l |= 1 << 2;
}

void CPU::set_2_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val | (u8) (1 << 2));
}

void CPU::set_2_a() {
    this->registers.af.b.h |= 1 << 2;
}

void CPU::set_3_b() {
    this->registers.bc.b.h |= 1 << 3;
}

void CPU::set_3_c() {
    this->registers.bc.b.l |= 1 << 3;
}

void CPU::set_3_d() {
    this->registers.de.b.h |= 1 << 3;
}

void CPU::set_3_e() {
    this->registers.de.b.l |= 1 << 3;
}

void CPU::set_3_h() {
    this->registers.hl.b.h |= 1 << 3;
}

void CPU::set_3_l() {
    this->registers.hl.b.l |= 1 << 3;
}

void CPU::set_3_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val | (u8) (1 << 3));
}

void CPU::set_3_a() {
    this->registers.af.b.h |= 1 << 3;
}

void CPU::set_4_b() {
    this->registers.bc.b.h |= 1 << 4;
}

void CPU::set_4_c() {
    this->registers.bc.b.l |= 1 << 4;
}

void CPU::set_4_d() {
    this->registers.de.b.h |= 1 << 4;
}

void CPU::set_4_e() {
    this->registers.de.b.l |= 1 << 4;
}

void CPU::set_4_h() {
    this->registers.hl.b.h |= 1 << 4;
}

void CPU::set_4_l() {
    this->registers.hl.b.l |= 1 << 4;
}

void CPU::set_4_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val | (u8) (1 << 4));
}

void CPU::set_4_a() {
    this->registers.af.b.h |= 1 << 4;
}

void CPU::set_5_b() {
    this->registers.bc.b.h |= 1 << 5;
}

void CPU::set_5_c() {
    this->registers.bc.b.l |= 1 << 5;
}

void CPU::set_5_d() {
    this->registers.de.b.h |= 1 << 5;
}

void CPU::set_5_e() {
    this->registers.de.b.l |= 1 << 5;
}

void CPU::set_5_h() {
    this->registers.hl.b.h |= 1 << 5;
}

void CPU::set_5_l() {
    this->registers.hl.b.l |= 1 << 5;
}

void CPU::set_5_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val | (u8) (1 << 5));
}

void CPU::set_5_a() {
    this->registers.af.b.h |= 1 << 5;
}

void CPU::set_6_b() {
    this->registers.bc.b.h |= 1 << 6;
}

void CPU::set_6_c() {
    this->registers.bc.b.l |= 1 << 6;
}

void CPU::set_6_d() {
    this->registers.de.b.h |= 1 << 6;
}

void CPU::set_6_e() {
    this->registers.de.b.l |= 1 << 6;
}

void CPU::set_6_h() {
    this->registers.hl.b.h |= 1 << 6;
}

void CPU::set_6_l() {
    this->registers.hl.b.l |= 1 << 6;
}

void CPU::set_6_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val | (u8) (1 << 6));
}

void CPU::set_6_a() {
    this->registers.af.b.h |= 1 << 6;
}

void CPU::set_7_b() {
    this->registers.bc.b.h |= 1 << 7;
}

void CPU::set_7_c() {
    this->registers.bc.b.l |= 1 << 7;
}

void CPU::set_7_d() {
    this->registers.de.b.h |= 1 << 7;
}

void CPU::set_7_e() {
    this->registers.de.b.l |= 1 << 7;
}

void CPU::set_7_h() {
    this->registers.hl.b.h |= 1 << 7;
}

void CPU::set_7_l() {
    this->registers.hl.b.l |= 1 << 7;
}

void CPU::set_7_hlp() {
    u8 val = MEMREAD(this->registers.hl.w);
    MEMWRITE(this->registers.hl.w, val | (u8) (1 << 7));
}

void CPU::set_7_a() {
    this->registers.af.b.h |= 1 << 7;
}
