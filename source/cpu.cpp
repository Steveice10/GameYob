#include <string.h>
#include <cpu.h>

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

#define FLAG_GET(f) ((this->registers.r8[R8_F] & (f)) == (f))
#define FLAG_SET(f, x) (this->registers.r8[R8_F] ^= (-(x) ^ this->registers.r8[R8_F]) & (f))

#define SETPC(val) (this->registers.r16[R16_PC] = (val), this->advanceCycles(4))

#define MEMREAD(addr) (temp1 = this->gameboy->mmu->read(addr), this->advanceCycles(4), temp1)
#define MEMWRITE(addr, val) (this->gameboy->mmu->write(addr, val), this->advanceCycles(4))

#define READPC8() MEMREAD(this->registers.r16[R16_PC]++)
#define READPC16() (temp2 = READPC8(), temp2 | (READPC8() << 8))

#define PUSH(val) (MEMWRITE(--this->registers.r16[R16_SP], ((val) >> 8)), MEMWRITE(--this->registers.r16[R16_SP], ((val) & 0xFF)))
#define POP() (temp2 = MEMREAD(this->registers.r16[R16_SP]++), temp2 | (MEMREAD(this->registers.r16[R16_SP]++) << 8))

#define ADD16(r, n) (this->advanceCycles(4), (r) + (n))
#define SUB16(r, n) (this->advanceCycles(4), (r) - (n))

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
    this->imeCycle = 0;

    if(this->gameboy->gbMode == MODE_CGB && this->gameboy->settings.gbaModeOption) {
        this->registers.r8[R8_B] = 1;
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
    data.read((char*) &this->imeCycle, sizeof(this->imeCycle));
}

void CPU::saveState(std::ostream& data) {
    data.write((char*) &this->cycleCount, sizeof(this->cycleCount));
    data.write((char*) &this->eventCycle, sizeof(this->eventCycle));
    data.write((char*) &this->registers, sizeof(this->registers));
    data.write((char*) &this->haltState, sizeof(this->haltState));
    data.write((char*) &this->haltBug, sizeof(this->haltBug));
    data.write((char*) &this->ime, sizeof(this->ime));
    data.write((char*) &this->imeCycle, sizeof(this->imeCycle));
}

static const u8 r[8] = {
        R8_B,
        R8_C,
        R8_D,
        R8_E,
        R8_H,
        R8_L,
        R16_HL,
        R8_A
};

static const u8 rp[4] = {
        R16_BC,
        R16_DE,
        R16_HL,
        R16_SP
};

static const u8 rp2[4] = {
        R16_BC,
        R16_DE,
        R16_HL,
        R16_AF
};

static const u8 cc[4] = {
    FLAG_ZERO,
    FLAG_ZERO,
    FLAG_CARRY,
    FLAG_CARRY
};

#define READ_R(i) ((i) == 6 ? MEMREAD(this->registers.r16[R16_HL]) : this->registers.r8[r[i]])
#define WRITE_R(i, v) ((i) == 6 ? (MEMWRITE(this->registers.r16[R16_HL], (v)), 0) : this->registers.r8[r[i]] = (v))

#define READ_RP(i) (this->registers.r16[rp[i]])
#define WRITE_RP(i, v) (this->registers.r16[rp[i]] = (v))

#define READ_RP2(i) (this->registers.r16[rp2[i]])
#define WRITE_RP2(i, v) (this->registers.r16[rp2[i]] = (v))

#define CHECK_CC(i) (FLAG_GET(cc[i]) ^ (~(i) & 1))

inline void CPU::alu(u8 func, u8 val) {
    u8 a = this->registers.r8[R8_A];

    u16 result = 0;

    if(func < 4 || func == 7) {
        u16 low = 0;
        u16 high = 0;

        switch(func) {
            case 1: { // ADC
                low = (u16) FLAG_GET(FLAG_CARRY);
            }
            case 0: { // ADD A
                low += (a & 0xF) + (val & 0xF);
                high = (a >> 4) + (val >> 4);
                break;
            }
            case 3: { // SBC
                low = (u16) -FLAG_GET(FLAG_CARRY);
            }
            case 2:   // SUB
            case 7: { // CP
                low += (a & 0xF) - (val & 0xF);
                high = (a >> 4) - (val >> 4);
                break;
            }
        }

        FLAG_SET(FLAG_NEGATIVE, (func & 0x6) != 0);
        FLAG_SET(FLAG_HALFCARRY, (low & 0xF0) != 0);

        result = low + (high << 4);
    } else {
        switch(func) {
            case 4: { // AND
                result = a & val;
                break;
            }
            case 5: { // XOR
                result = a ^ val;
                break;
            }
            case 6: { // OR
                result = a | val;
                break;
            }
        }

        FLAG_SET(FLAG_NEGATIVE, 0);
        FLAG_SET(FLAG_HALFCARRY, func == 4);
    }

    FLAG_SET(FLAG_CARRY, result >= 0x100);
    FLAG_SET(FLAG_ZERO, (result & 0xFF) == 0);

    if(func != 7) {
        this->registers.r8[R8_A] = (u8) (result & 0xFF);
    }
}

inline u8 CPU::rot(u8 func, u8 val) {
    u8 carry = 0;
    u8 result = 0;

    switch(func) {
        case 0: { // RLC
            carry = (u8) ((val & 0x80) >> 7);
            result = (val << 1) | carry;
            break;
        }
        case 1: { // RRC
            carry = (u8) ((val & 0x01) << 7);
            result = (val >> 1) | carry;
            break;
        }
        case 2: { // RL
            carry = (u8) (val & 0x80);
            result = (val << 1) | (u8) FLAG_GET(FLAG_CARRY);
            break;
        }
        case 3: { // RR
            carry = (u8) (val & 0x01);
            result = (val >> 1) | (u8) (FLAG_GET(FLAG_CARRY) << 7);
            break;
        }
        case 4: { // SLA
            carry = (u8) (val & 0x80);
            result = val << 1;
            break;
        }
        case 5: { // SRA
            carry = (u8) (val & 0x01);
            result = (val >> 1) | (u8) (val & 0x80);
            break;
        }
        case 6: { // SWAP
            carry = 0;
            result = (u8) (((val & 0x0F) << 4) | ((val & 0xF0) >> 4));
            break;
        }
        case 7: { // SRL
            carry = (u8) (val & 0x01);
            result = val >> 1;
            break;
        }
    }

    FLAG_SET(FLAG_NEGATIVE, 0);
    FLAG_SET(FLAG_HALFCARRY, 0);
    FLAG_SET(FLAG_CARRY, carry != 0);
    FLAG_SET(FLAG_ZERO, result == 0);

    return result;
}

void CPU::run() {
    if(!this->haltState) {
        u8 op = READPC8();

        if(this->haltBug) {
            this->registers.r16[R16_PC]--;
            this->haltBug = false;
        }

        u8 x = op >> 6;
        u8 y = (u8) ((op >> 3) & 0x7);
        u8 z = (u8) (op & 0x7);
        u8 p = y >> 1;
        u8 q = (u8) (y & 0x1);

        switch(x) {
            case 0: {
                switch(z) {
                    case 0: {
                        switch(y) {
                            case 0: { // NOP
                                break;
                            }
                            case 1: { // LD (nn), SP
                                u16 addr = READPC16();

                                MEMWRITE(addr, this->registers.r8[R8_SP_P]);
                                MEMWRITE((u16) (addr + 1), this->registers.r8[R8_SP_S]);
                                break;
                            }
                            case 2: { // STOP
                                u8 key1 = this->gameboy->mmu->readIO(KEY1);
                                if(this->gameboy->gbMode == MODE_CGB && (key1 & 0x01) != 0) {
                                    bool doubleSpeed = (key1 & 0x80) == 0;

                                    this->gameboy->apu->setHalfSpeed(doubleSpeed);
                                    this->gameboy->ppu->setHalfSpeed(doubleSpeed);

                                    this->gameboy->mmu->writeIO(KEY1, (u8) (key1 ^ 0x81));
                                    this->registers.r16[R16_PC]++;
                                } else {
                                    this->haltState = true;
                                }

                                break;
                            }
                            case 3: { // JR d
                                s8 offset = READPC8();
                                SETPC(this->registers.r16[R16_PC] + offset);
                                break;
                            }
                            default: { // JR cc[y-4], d
                                s8 offset = READPC8();
                                if(CHECK_CC(y - 4)) {
                                    SETPC(this->registers.r16[R16_PC] + offset);
                                }

                                break;
                            }
                        }

                        break;
                    }
                    case 1: {
                        switch(q) {
                            case 0: { // LD rp[p], nn
                                u16 val = READPC16();
                                WRITE_RP(p, val);
                                break;
                            }
                            case 1: { // ADD HL, rp[p]
                                u16 hl = this->registers.r16[R16_HL];
                                u16 val = READ_RP(p);

                                u32 result = ADD16(hl, val);
                                FLAG_SET(FLAG_NEGATIVE, 0);
                                FLAG_SET(FLAG_HALFCARRY, (hl & 0xFFF) + (val & 0xFFF) >= 0x1000);
                                FLAG_SET(FLAG_CARRY, result >= 0x10000);

                                this->registers.r16[R16_HL] = (u16) (result & 0xFFFF);
                                break;
                            }
                        }

                        break;
                    }
                    case 2: {
                        switch(q) {
                            case 0: {
                                switch(p) {
                                    case 0: { // LD (BC), A
                                        MEMWRITE(this->registers.r16[R16_BC], this->registers.r8[R8_A]);
                                        break;
                                    }
                                    case 1: { // LD (DE), A
                                        MEMWRITE(this->registers.r16[R16_DE], this->registers.r8[R8_A]);
                                        break;
                                    }
                                    case 2: { // LD (HL+), A
                                        MEMWRITE(this->registers.r16[R16_HL]++, this->registers.r8[R8_A]);
                                        break;
                                    }
                                    case 3: { // LD (HL-), A
                                        MEMWRITE(this->registers.r16[R16_HL]--, this->registers.r8[R8_A]);
                                        break;
                                    }
                                }

                                break;
                            }
                            case 1: {
                                switch(p) {
                                    case 0: { // LD A, (BC)
                                        this->registers.r8[R8_A] = MEMREAD(this->registers.r16[R16_BC]);
                                        break;
                                    }
                                    case 1: { // LD A, (DE)
                                        this->registers.r8[R8_A] = MEMREAD(this->registers.r16[R16_DE]);
                                        break;
                                    }
                                    case 2: { // LD A, (HL+)
                                        this->registers.r8[R8_A] = MEMREAD(this->registers.r16[R16_HL]++);
                                        break;
                                    }
                                    case 3: { // LD A, (HL-)
                                        this->registers.r8[R8_A] = MEMREAD(this->registers.r16[R16_HL]--);
                                        break;
                                    }
                                }

                                break;
                            }
                        }

                        break;
                    }
                    case 3: {
                        switch(q) {
                            case 0: { // INC rp[p]
                                u16 val = READ_RP(p);
                                u16 result = (u16) ADD16(val, 1);
                                WRITE_RP(p, result);
                                break;
                            }
                            case 1: { // DEC rp[p]
                                u16 val = READ_RP(p);
                                u16 result = (u16) SUB16(val, 1);
                                WRITE_RP(p, result);
                                break;
                            }
                        }

                        break;
                    }
                    case 4: { // INC r[y]
                        u8 val = READ_R(y);

                        u8 result = (u8) (val + 1);
                        FLAG_SET(FLAG_NEGATIVE, 0);
                        FLAG_SET(FLAG_HALFCARRY, (val & 0xF) == 0xF);
                        FLAG_SET(FLAG_ZERO, result == 0);

                        WRITE_R(y, result);
                        break;
                    }
                    case 5: { // DEC r[y]
                        u8 val = READ_R(y);

                        u8 result = (u8) (val - 1);
                        FLAG_SET(FLAG_NEGATIVE, 1);
                        FLAG_SET(FLAG_HALFCARRY, (val & 0xF) == 0);
                        FLAG_SET(FLAG_ZERO, result == 0);

                        WRITE_R(y, result);
                        break;
                    }
                    case 6: { // LD r[y], n
                        u8 val = READPC8();
                        WRITE_R(y, val);
                        break;
                    }
                    case 7: {
                        switch(y) {
                            case 4: { // DAA
                                u16 result = this->registers.r8[R8_A];

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

                                this->registers.r8[R8_A] = (u8) (result & 0xFF);
                                break;
                            }
                            case 5: { // CPL
                                this->registers.r8[R8_A] = ~this->registers.r8[R8_A];
                                FLAG_SET(FLAG_NEGATIVE, 1);
                                FLAG_SET(FLAG_HALFCARRY, 1);
                                break;
                            }
                            case 6: { // SCF
                                FLAG_SET(FLAG_NEGATIVE, 0);
                                FLAG_SET(FLAG_HALFCARRY, 0);
                                FLAG_SET(FLAG_CARRY, 1);
                                break;
                            }
                            case 7: { // CCF
                                FLAG_SET(FLAG_CARRY, !FLAG_GET(FLAG_CARRY));
                                FLAG_SET(FLAG_NEGATIVE, 0);
                                FLAG_SET(FLAG_HALFCARRY, 0);
                                break;
                            }
                            default: { // rot[y] A
                                this->registers.r8[R8_A] = this->rot(y, this->registers.r8[R8_A]);
                                FLAG_SET(FLAG_ZERO, 0);
                                break;
                            }
                        }

                        break;
                    }
                }

                break;
            }
            case 1: {
                if(z == 6 && y == 6) { // HALT
                    if(!this->ime && (this->gameboy->mmu->readIO(IF) & this->gameboy->mmu->readIO(IE) & 0x1F) != 0) {
                        if(this->gameboy->gbMode != MODE_CGB) {
                            this->haltBug = true;
                        }
                    } else {
                        this->haltState = true;
                    }
                } else { // LD r[y], r[z]
                    u8 val = READ_R(z);
                    WRITE_R(y, val);
                }

                break;
            }
            case 2: { // alu[y] r[z]
                this->alu(y, READ_R(z));
                break;
            }
            case 3: {
                switch(z) {
                    case 0: {
                        switch(y) {
                            case 4: { // LD (0xFF00 + nn), A
                                u8 reg = READPC8();
                                MEMWRITE((u16) (0xFF00 + reg), this->registers.r8[R8_A]);
                                break;
                            }
                            case 5: { // ADD SP, d
                                u16 sp = this->registers.r16[R16_SP];
                                u8 val = READPC8();

                                u16 result = (u16) ADD16(sp, (s8) val);
                                FLAG_SET(FLAG_NEGATIVE, 0);
                                FLAG_SET(FLAG_HALFCARRY, (sp & 0xF) + (val & 0xF) >= 0x10);
                                FLAG_SET(FLAG_CARRY, (sp & 0xFF) + val >= 0x100);
                                FLAG_SET(FLAG_ZERO, 0);

                                this->registers.r16[R16_SP] = result;
                                this->advanceCycles(4);
                                break;
                            }
                            case 6: { // LD A, (0xFF00 + n)
                                u8 reg = READPC8();
                                this->registers.r8[R8_A] = MEMREAD((u16) (0xFF00 + reg));
                                break;
                            }
                            case 7: { // LD HL, SP+ d
                                u16 sp = this->registers.r16[R16_SP];
                                u8 val = READPC8();

                                u16 result = (u16) ADD16(sp, (s8) val);
                                FLAG_SET(FLAG_NEGATIVE, 0);
                                FLAG_SET(FLAG_HALFCARRY, (sp & 0xF) + (val & 0xF) >= 0x10);
                                FLAG_SET(FLAG_CARRY, (sp & 0xFF) + val >= 0x100);
                                FLAG_SET(FLAG_ZERO, 0);

                                this->registers.r16[R16_HL] = result;
                                break;
                            }
                            default: { // RET cc[y]
                                this->advanceCycles(4);

                                if(CHECK_CC(y)) {
                                    u16 addr = POP();
                                    SETPC(addr);
                                }

                                break;
                            }
                        }

                        break;
                    }
                    case 1: {
                        switch(q) {
                            case 0: { // POP rp2[p]
                                u8 oldFLow = (u8) (this->registers.r8[R8_F] & 0xF);

                                u16 val = POP();
                                WRITE_RP2(p, val);

                                this->registers.r8[R8_F] = (u8) ((this->registers.r8[R8_F] & 0xF0) | oldFLow);
                                break;
                            }
                            case 1: {
                                switch(p) {
                                    case 0: { // RET
                                        u16 addr = POP();
                                        SETPC(addr);
                                        break;
                                    }
                                    case 1: { // RETI
                                        u16 addr = POP();

                                        this->imeCycle = this->cycleCount + 4;
                                        this->setEventCycle(this->imeCycle);

                                        SETPC(addr);
                                        break;
                                    }
                                    case 2: { // JP HL
                                        this->registers.r16[R16_PC] = this->registers.r16[R16_HL];
                                        break;
                                    }
                                    case 3: { // LD SP, HL
                                        this->registers.r16[R16_SP] = this->registers.r16[R16_HL];
                                        this->advanceCycles(4);
                                        break;
                                    }
                                }

                                break;
                            }
                        }

                        break;
                    }
                    case 2: {
                        switch(y) {
                            case 4: { // LD (0xFF00+C), A
                                MEMWRITE((u16) (0xFF00 + this->registers.r8[R8_C]), this->registers.r8[R8_A]);
                                break;
                            }
                            case 5: { // LD (nn), A
                                u16 addr = READPC16();
                                MEMWRITE(addr, this->registers.r8[R8_A]);
                                break;
                            }
                            case 6: { // LD A, (0xFF00+C)
                                this->registers.r8[R8_A] = MEMREAD((u16) (0xFF00 + this->registers.r8[R8_C]));
                                break;
                            }
                            case 7: { // LD A, (nn)
                                u16 addr = READPC16();
                                this->registers.r8[R8_A] = MEMREAD(addr);
                                break;
                            }
                            default: { // JP cc[y], nn
                                u16 addr = READPC16();
                                if(CHECK_CC(y)) {
                                    SETPC(addr);
                                }

                                break;
                            }
                        }

                        break;
                    }
                    case 3: {
                        switch(y) {
                            case 0: { // JP nn
                                u16 addr = READPC16();
                                SETPC(addr);
                                break;
                            }
                            case 1: { // CB
                                u8 cbOp = READPC8();

                                u8 cbX = cbOp >> 6;
                                u8 cbY = (u8) ((cbOp >> 3) & 0x7);
                                u8 cbZ = (u8) (cbOp & 0x7);

                                u8 val = READ_R(cbZ);
                                u8 result = 0;

                                switch(cbX) {
                                    case 0: { // rot[y] r[z]
                                        result = this->rot(cbY, val);
                                        break;
                                    }
                                    case 1: { // BIT y, r[z]
                                        FLAG_SET(FLAG_NEGATIVE, 0);
                                        FLAG_SET(FLAG_HALFCARRY, 1);
                                        FLAG_SET(FLAG_ZERO, (val & (1 << cbY)) == 0);
                                        break;
                                    }
                                    case 2: { // RES y, r[z]
                                        result = val & ~((u8) (1 << cbY));
                                        break;
                                    }
                                    case 3: { // SET y, r[z]
                                        result = val | (u8) (1 << cbY);
                                        break;
                                    }
                                }

                                if(cbX != 1) {
                                    WRITE_R(cbZ, result);
                                }

                                break;
                            }
                            case 6: { // DI
                                this->ime = false;
                                this->imeCycle = 0;
                                break;
                            }
                            case 7: { // EI
                                this->imeCycle = this->cycleCount + 4;
                                this->setEventCycle(this->imeCycle);
                                break;
                            }
                        }

                        break;
                    }
                    case 4: {
                        if((y & 0x4) == 0) { // CALL cc[y], nn
                            u16 addr = READPC16();
                            if(CHECK_CC(y)) {
                                PUSH(this->registers.r16[R16_PC]);
                                SETPC(addr);
                            }
                        }

                        break;
                    }
                    case 5: {
                        switch(q) {
                            case 0: { // PUSH rp2[p]
                                u16 val = READ_RP2(p);
                                PUSH(val);
                                this->advanceCycles(4);
                                break;
                            }
                            case 1: {
                                if(p == 0) { // CALL nn
                                    u16 addr = READPC16();
                                    PUSH(this->registers.r16[R16_PC]);
                                    SETPC(addr);
                                }

                                break;
                            }
                        }

                        break;
                    }
                    case 6: { // alu[y] n
                        u8 val = READPC8();
                        this->alu(y, val);
                        break;
                    }
                    case 7: { // RST y*8
                        PUSH(this->registers.r16[R16_PC]);
                        SETPC(y * 8);
                        break;
                    }
                }

                break;
            }
        }
    } else {
        this->advanceCycles(this->eventCycle - this->cycleCount);
    }

    int triggered = this->gameboy->mmu->readIO(IF) & this->gameboy->mmu->readIO(IE);
    if(triggered != 0) {
        this->haltState = false;
        if(this->ime) {
            this->ime = false;

            this->advanceCycles(12);

            PUSH(this->registers.r16[R16_PC]);

            int irqNo = __builtin_ffs(triggered) - 1;
            this->registers.r16[R16_PC] = (u16) (0x40 + (irqNo << 3));
            this->gameboy->mmu->writeIO(IF, (u8) (this->gameboy->mmu->readIO(IF) & ~(1 << irqNo)));
        }
    }
}

void CPU::updateEvents() {
    if(this->imeCycle != 0) {
        if(this->imeCycle >= this->cycleCount) {
            this->imeCycle = 0;
            this->ime = true;
        } else {
            this->setEventCycle(this->imeCycle);
        }
    }

    if(this->gameboy->cartridge != NULL) {
        this->gameboy->cartridge->update();
    }

    this->gameboy->ppu->update();
    this->gameboy->apu->update();
    this->gameboy->sgb->update();
    this->gameboy->timer->update();
    this->gameboy->serial->update();
}