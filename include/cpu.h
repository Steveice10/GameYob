#pragma once

#include "types.h"

class Gameboy;

#define INT_VBLANK 0x01
#define INT_LCD 0x02
#define INT_TIMER 0x04
#define INT_SERIAL 0x08
#define INT_JOYPAD 0x10

typedef union {
    u16 w;
    struct B {
        u8 l;
        u8 h;
    } b;
} Register;

struct Registers {
    Register sp;
    Register pc;
    Register af;
    Register bc;
    Register de;
    Register hl;
};

class CPU {
public:
    CPU(Gameboy* gameboy);

    void reset();

    void loadState(FILE* file, int version);
    void saveState(FILE* file);

    int run();

    void advanceCycles(u64 cycles);

    void setDoubleSpeed(bool doubleSpeed);

    void requestInterrupt(int id);

    inline u64 getCycle() {
        return this->cycleCount;
    }

    inline void setEventCycle(u64 cycle) {
        if(cycle < this->eventCycle) {
            this->eventCycle = cycle > this->cycleCount ? cycle : this->cycleCount;
        }
    }
private:
    void undefined();
    void nop();
    void ld_bc_nn();
    void ld_bcp_a();
    void inc_bc();
    void inc_b();
    void dec_b();
    void ld_b_n();
    void rlca();
    void ld_nnp_sp();
    void add_hl_bc();
    void ld_a_bcp();
    void dec_bc();
    void inc_c();
    void dec_c();
    void ld_c_n();
    void rrca();
    void stop();
    void ld_de_nn();
    void ld_dep_a();
    void inc_de();
    void inc_d();
    void dec_d();
    void ld_d_n();
    void rla();
    void jr_n();
    void add_hl_de();
    void ld_a_dep();
    void dec_de();
    void inc_e();
    void dec_e();
    void ld_e_n();
    void rra();
    void jr_nz_n();
    void ld_hl_nn();
    void ldi_hlp_a();
    void inc_hl();
    void inc_h();
    void dec_h();
    void ld_h_n();
    void daa();
    void jr_z_n();
    void add_hl_hl();
    void ldi_a_hlp();
    void dec_hl();
    void inc_l();
    void dec_l();
    void ld_l_n();
    void cpl();
    void jr_nc_n();
    void ld_sp_nn();
    void ldd_hlp_a();
    void inc_sp();
    void inc_hlp();
    void dec_hlp();
    void ld_hlp_n();
    void scf();
    void jr_c_n();
    void add_hl_sp();
    void ldd_a_hlp();
    void dec_sp();
    void inc_a();
    void dec_a();
    void ld_a_n();
    void ccf();
    void ld_b_c();
    void ld_b_d();
    void ld_b_e();
    void ld_b_h();
    void ld_b_l();
    void ld_b_hlp();
    void ld_b_a();
    void ld_c_b();
    void ld_c_d();
    void ld_c_e();
    void ld_c_h();
    void ld_c_l();
    void ld_c_hlp();
    void ld_c_a();
    void ld_d_b();
    void ld_d_c();
    void ld_d_e();
    void ld_d_h();
    void ld_d_l();
    void ld_d_hlp();
    void ld_d_a();
    void ld_e_b();
    void ld_e_c();
    void ld_e_d();
    void ld_e_h();
    void ld_e_l();
    void ld_e_hlp();
    void ld_e_a();
    void ld_h_b();
    void ld_h_c();
    void ld_h_d();
    void ld_h_e();
    void ld_h_l();
    void ld_h_hlp();
    void ld_h_a();
    void ld_l_b();
    void ld_l_c();
    void ld_l_d();
    void ld_l_e();
    void ld_l_h();
    void ld_l_hlp();
    void ld_l_a();
    void ld_hlp_b();
    void ld_hlp_c();
    void ld_hlp_d();
    void ld_hlp_e();
    void ld_hlp_h();
    void ld_hlp_l();
    void halt();
    void ld_hlp_a();
    void ld_a_b();
    void ld_a_c();
    void ld_a_d();
    void ld_a_e();
    void ld_a_h();
    void ld_a_l();
    void ld_a_hlp();
    void add_a_b();
    void add_a_c();
    void add_a_d();
    void add_a_e();
    void add_a_h();
    void add_a_l();
    void add_a_hlp();
    void add_a_a();
    void adc_b();
    void adc_c();
    void adc_d();
    void adc_e();
    void adc_h();
    void adc_l();
    void adc_hlp();
    void adc_a();
    void sub_b();
    void sub_c();
    void sub_d();
    void sub_e();
    void sub_h();
    void sub_l();
    void sub_hlp();
    void sub_a();
    void sbc_b();
    void sbc_c();
    void sbc_d();
    void sbc_e();
    void sbc_h();
    void sbc_l();
    void sbc_hlp();
    void sbc_a();
    void and_b();
    void and_c();
    void and_d();
    void and_e();
    void and_h();
    void and_l();
    void and_hlp();
    void and_a();
    void xor_b();
    void xor_c();
    void xor_d();
    void xor_e();
    void xor_h();
    void xor_l();
    void xor_hlp();
    void xor_a();
    void or_b();
    void or_c();
    void or_d();
    void or_e();
    void or_h();
    void or_l();
    void or_hlp();
    void or_a();
    void cp_b();
    void cp_c();
    void cp_d();
    void cp_e();
    void cp_h();
    void cp_l();
    void cp_hlp();
    void cp_a();
    void ret_nz();
    void pop_bc();
    void jp_nz_nn();
    void jp_nn();
    void call_nz_nn();
    void push_bc();
    void add_a_n();
    void rst_0();
    void ret_z();
    void ret();
    void jp_z_nn();
    void cb_n();
    void call_z_nn();
    void call_nn();
    void adc_n();
    void rst_08();
    void ret_nc();
    void pop_de();
    void jp_nc_nn();
    void call_nc_nn();
    void push_de();
    void sub_n();
    void rst_10();
    void ret_c();
    void reti();
    void jp_c_nn();
    void call_c_nn();
    void sbc_n();
    void rst_18();
    void ld_ff_n_a();
    void pop_hl();
    void ld_ff_c_a();
    void push_hl();
    void and_n();
    void rst_20();
    void add_sp_n();
    void jp_hl();
    void ld_nnp_a();
    void xor_n();
    void rst_28();
    void ld_a_ff_n();
    void pop_af();
    void ld_a_ff_c();
    void di_inst();
    void push_af();
    void or_n();
    void rst_30();
    void ld_hl_sp_n();
    void ld_sp_hl();
    void ld_a_nnp();
    void ei();
    void cp_n();
    void rst_38();

    void rlc_b();
    void rlc_c();
    void rlc_d();
    void rlc_e();
    void rlc_h();
    void rlc_l();
    void rlc_hlp();
    void rlc_a();
    void rrc_b();
    void rrc_c();
    void rrc_d();
    void rrc_e();
    void rrc_h();
    void rrc_l();
    void rrc_hlp();
    void rrc_a();
    void rl_b();
    void rl_c();
    void rl_d();
    void rl_e();
    void rl_h();
    void rl_l();
    void rl_hlp();
    void rl_a();
    void rr_b();
    void rr_c();
    void rr_d();
    void rr_e();
    void rr_h();
    void rr_l();
    void rr_hlp();
    void rr_a();
    void sla_b();
    void sla_c();
    void sla_d();
    void sla_e();
    void sla_h();
    void sla_l();
    void sla_hlp();
    void sla_a();
    void sra_b();
    void sra_c();
    void sra_d();
    void sra_e();
    void sra_h();
    void sra_l();
    void sra_hlp();
    void sra_a();
    void swap_b();
    void swap_c();
    void swap_d();
    void swap_e();
    void swap_h();
    void swap_l();
    void swap_hlp();
    void swap_a();
    void srl_b();
    void srl_c();
    void srl_d();
    void srl_e();
    void srl_h();
    void srl_l();
    void srl_hlp();
    void srl_a();
    void bit_0_b();
    void bit_0_c();
    void bit_0_d();
    void bit_0_e();
    void bit_0_h();
    void bit_0_l();
    void bit_0_hlp();
    void bit_0_a();
    void bit_1_b();
    void bit_1_c();
    void bit_1_d();
    void bit_1_e();
    void bit_1_h();
    void bit_1_l();
    void bit_1_hlp();
    void bit_1_a();
    void bit_2_b();
    void bit_2_c();
    void bit_2_d();
    void bit_2_e();
    void bit_2_h();
    void bit_2_l();
    void bit_2_hlp();
    void bit_2_a();
    void bit_3_b();
    void bit_3_c();
    void bit_3_d();
    void bit_3_e();
    void bit_3_h();
    void bit_3_l();
    void bit_3_hlp();
    void bit_3_a();
    void bit_4_b();
    void bit_4_c();
    void bit_4_d();
    void bit_4_e();
    void bit_4_h();
    void bit_4_l();
    void bit_4_hlp();
    void bit_4_a();
    void bit_5_b();
    void bit_5_c();
    void bit_5_d();
    void bit_5_e();
    void bit_5_h();
    void bit_5_l();
    void bit_5_hlp();
    void bit_5_a();
    void bit_6_b();
    void bit_6_c();
    void bit_6_d();
    void bit_6_e();
    void bit_6_h();
    void bit_6_l();
    void bit_6_hlp();
    void bit_6_a();
    void bit_7_b();
    void bit_7_c();
    void bit_7_d();
    void bit_7_e();
    void bit_7_h();
    void bit_7_l();
    void bit_7_hlp();
    void bit_7_a();
    void res_0_b();
    void res_0_c();
    void res_0_d();
    void res_0_e();
    void res_0_h();
    void res_0_l();
    void res_0_hlp();
    void res_0_a();
    void res_1_b();
    void res_1_c();
    void res_1_d();
    void res_1_e();
    void res_1_h();
    void res_1_l();
    void res_1_hlp();
    void res_1_a();
    void res_2_b();
    void res_2_c();
    void res_2_d();
    void res_2_e();
    void res_2_h();
    void res_2_l();
    void res_2_hlp();
    void res_2_a();
    void res_3_b();
    void res_3_c();
    void res_3_d();
    void res_3_e();
    void res_3_h();
    void res_3_l();
    void res_3_hlp();
    void res_3_a();
    void res_4_b();
    void res_4_c();
    void res_4_d();
    void res_4_e();
    void res_4_h();
    void res_4_l();
    void res_4_hlp();
    void res_4_a();
    void res_5_b();
    void res_5_c();
    void res_5_d();
    void res_5_e();
    void res_5_h();
    void res_5_l();
    void res_5_hlp();
    void res_5_a();
    void res_6_b();
    void res_6_c();
    void res_6_d();
    void res_6_e();
    void res_6_h();
    void res_6_l();
    void res_6_hlp();
    void res_6_a();
    void res_7_b();
    void res_7_c();
    void res_7_d();
    void res_7_e();
    void res_7_h();
    void res_7_l();
    void res_7_hlp();
    void res_7_a();
    void set_0_b();
    void set_0_c();
    void set_0_d();
    void set_0_e();
    void set_0_h();
    void set_0_l();
    void set_0_hlp();
    void set_0_a();
    void set_1_b();
    void set_1_c();
    void set_1_d();
    void set_1_e();
    void set_1_h();
    void set_1_l();
    void set_1_hlp();
    void set_1_a();
    void set_2_b();
    void set_2_c();
    void set_2_d();
    void set_2_e();
    void set_2_h();
    void set_2_l();
    void set_2_hlp();
    void set_2_a();
    void set_3_b();
    void set_3_c();
    void set_3_d();
    void set_3_e();
    void set_3_h();
    void set_3_l();
    void set_3_hlp();
    void set_3_a();
    void set_4_b();
    void set_4_c();
    void set_4_d();
    void set_4_e();
    void set_4_h();
    void set_4_l();
    void set_4_hlp();
    void set_4_a();
    void set_5_b();
    void set_5_c();
    void set_5_d();
    void set_5_e();
    void set_5_h();
    void set_5_l();
    void set_5_hlp();
    void set_5_a();
    void set_6_b();
    void set_6_c();
    void set_6_d();
    void set_6_e();
    void set_6_h();
    void set_6_l();
    void set_6_hlp();
    void set_6_a();
    void set_7_b();
    void set_7_c();
    void set_7_d();
    void set_7_e();
    void set_7_h();
    void set_7_l();
    void set_7_hlp();
    void set_7_a();

    void (CPU::*opcodes[0x100])() = {
            &CPU::nop,        // 0x00
            &CPU::ld_bc_nn,   // 0x01
            &CPU::ld_bcp_a,   // 0x02
            &CPU::inc_bc,     // 0x03
            &CPU::inc_b,      // 0x04
            &CPU::dec_b,      // 0x05
            &CPU::ld_b_n,     // 0x06
            &CPU::rlca,       // 0x07
            &CPU::ld_nnp_sp,  // 0x08
            &CPU::add_hl_bc,  // 0x09
            &CPU::ld_a_bcp,   // 0x0a
            &CPU::dec_bc,     // 0x0b
            &CPU::inc_c,      // 0x0c
            &CPU::dec_c,      // 0x0d
            &CPU::ld_c_n,     // 0x0e
            &CPU::rrca,       // 0x0f
            &CPU::stop,       // 0x10
            &CPU::ld_de_nn,   // 0x11
            &CPU::ld_dep_a,   // 0x12
            &CPU::inc_de,     // 0x13
            &CPU::inc_d,      // 0x14
            &CPU::dec_d,      // 0x15
            &CPU::ld_d_n,     // 0x16
            &CPU::rla,        // 0x17
            &CPU::jr_n,       // 0x18
            &CPU::add_hl_de,  // 0x19
            &CPU::ld_a_dep,   // 0x1a
            &CPU::dec_de,     // 0x1b
            &CPU::inc_e,      // 0x1c
            &CPU::dec_e,      // 0x1d
            &CPU::ld_e_n,     // 0x1e
            &CPU::rra,        // 0x1f
            &CPU::jr_nz_n,    // 0x20
            &CPU::ld_hl_nn,   // 0x21
            &CPU::ldi_hlp_a,  // 0x22
            &CPU::inc_hl,     // 0x23
            &CPU::inc_h,      // 0x24
            &CPU::dec_h,      // 0x25
            &CPU::ld_h_n,     // 0x26
            &CPU::daa,        // 0x27
            &CPU::jr_z_n,     // 0x28
            &CPU::add_hl_hl,  // 0x29
            &CPU::ldi_a_hlp,  // 0x2a
            &CPU::dec_hl,     // 0x2b
            &CPU::inc_l,      // 0x2c
            &CPU::dec_l,      // 0x2d
            &CPU::ld_l_n,     // 0x2e
            &CPU::cpl,        // 0x2f
            &CPU::jr_nc_n,    // 0x30
            &CPU::ld_sp_nn,   // 0x31
            &CPU::ldd_hlp_a,  // 0x32
            &CPU::inc_sp,     // 0x33
            &CPU::inc_hlp,    // 0x34
            &CPU::dec_hlp,    // 0x35
            &CPU::ld_hlp_n,   // 0x36
            &CPU::scf,        // 0x37
            &CPU::jr_c_n,     // 0x38
            &CPU::add_hl_sp,  // 0x39
            &CPU::ldd_a_hlp,  // 0x3a
            &CPU::dec_sp,     // 0x3b
            &CPU::inc_a,      // 0x3c
            &CPU::dec_a,      // 0x3d
            &CPU::ld_a_n,     // 0x3e
            &CPU::ccf,        // 0x3f
            &CPU::nop,        // 0x40
            &CPU::ld_b_c,     // 0x41
            &CPU::ld_b_d,     // 0x42
            &CPU::ld_b_e,     // 0x43
            &CPU::ld_b_h,     // 0x44
            &CPU::ld_b_l,     // 0x45
            &CPU::ld_b_hlp,   // 0x46
            &CPU::ld_b_a,     // 0x47
            &CPU::ld_c_b,     // 0x48
            &CPU::nop,        // 0x49
            &CPU::ld_c_d,     // 0x4a
            &CPU::ld_c_e,     // 0x4b
            &CPU::ld_c_h,     // 0x4c
            &CPU::ld_c_l,     // 0x4d
            &CPU::ld_c_hlp,   // 0x4e
            &CPU::ld_c_a,     // 0x4f
            &CPU::ld_d_b,     // 0x50
            &CPU::ld_d_c,     // 0x51
            &CPU::nop,        // 0x52
            &CPU::ld_d_e,     // 0x53
            &CPU::ld_d_h,     // 0x54
            &CPU::ld_d_l,     // 0x55
            &CPU::ld_d_hlp,   // 0x56
            &CPU::ld_d_a,     // 0x57
            &CPU::ld_e_b,     // 0x58
            &CPU::ld_e_c,     // 0x59
            &CPU::ld_e_d,     // 0x5a
            &CPU::nop,        // 0x5b
            &CPU::ld_e_h,     // 0x5c
            &CPU::ld_e_l,     // 0x5d
            &CPU::ld_e_hlp,   // 0x5e
            &CPU::ld_e_a,     // 0x5f
            &CPU::ld_h_b,     // 0x60
            &CPU::ld_h_c,     // 0x61
            &CPU::ld_h_d,     // 0x62
            &CPU::ld_h_e,     // 0x63
            &CPU::nop,        // 0x64
            &CPU::ld_h_l,     // 0x65
            &CPU::ld_h_hlp,   // 0x66
            &CPU::ld_h_a,     // 0x67
            &CPU::ld_l_b,     // 0x68
            &CPU::ld_l_c,     // 0x69
            &CPU::ld_l_d,     // 0x6a
            &CPU::ld_l_e,     // 0x6b
            &CPU::ld_l_h,     // 0x6c
            &CPU::nop,        // 0x6d
            &CPU::ld_l_hlp,   // 0x6e
            &CPU::ld_l_a,     // 0x6f
            &CPU::ld_hlp_b,   // 0x70
            &CPU::ld_hlp_c,   // 0x71
            &CPU::ld_hlp_d,   // 0x72
            &CPU::ld_hlp_e,   // 0x73
            &CPU::ld_hlp_h,   // 0x74
            &CPU::ld_hlp_l,   // 0x75
            &CPU::halt,       // 0x76
            &CPU::ld_hlp_a,   // 0x77
            &CPU::ld_a_b,     // 0x78
            &CPU::ld_a_c,     // 0x79
            &CPU::ld_a_d,     // 0x7a
            &CPU::ld_a_e,     // 0x7b
            &CPU::ld_a_h,     // 0x7c
            &CPU::ld_a_l,     // 0x7d
            &CPU::ld_a_hlp,   // 0x7e
            &CPU::nop,        // 0x7f
            &CPU::add_a_b,    // 0x80
            &CPU::add_a_c,    // 0x81
            &CPU::add_a_d,    // 0x82
            &CPU::add_a_e,    // 0x83
            &CPU::add_a_h,    // 0x84
            &CPU::add_a_l,    // 0x85
            &CPU::add_a_hlp,  // 0x86
            &CPU::add_a_a,    // 0x87
            &CPU::adc_b,      // 0x88
            &CPU::adc_c,      // 0x89
            &CPU::adc_d,      // 0x8a
            &CPU::adc_e,      // 0x8b
            &CPU::adc_h,      // 0x8c
            &CPU::adc_l,      // 0x8d
            &CPU::adc_hlp,    // 0x8e
            &CPU::adc_a,      // 0x8f
            &CPU::sub_b,      // 0x90
            &CPU::sub_c,      // 0x91
            &CPU::sub_d,      // 0x92
            &CPU::sub_e,      // 0x93
            &CPU::sub_h,      // 0x94
            &CPU::sub_l,      // 0x95
            &CPU::sub_hlp,    // 0x96
            &CPU::sub_a,      // 0x97
            &CPU::sbc_b,      // 0x98
            &CPU::sbc_c,      // 0x99
            &CPU::sbc_d,      // 0x9a
            &CPU::sbc_e,      // 0x9b
            &CPU::sbc_h,      // 0x9c
            &CPU::sbc_l,      // 0x9d
            &CPU::sbc_hlp,    // 0x9e
            &CPU::sbc_a,      // 0x9f
            &CPU::and_b,      // 0xa0
            &CPU::and_c,      // 0xa1
            &CPU::and_d,      // 0xa2
            &CPU::and_e,      // 0xa3
            &CPU::and_h,      // 0xa4
            &CPU::and_l,      // 0xa5
            &CPU::and_hlp,    // 0xa6
            &CPU::and_a,      // 0xa7
            &CPU::xor_b,      // 0xa8
            &CPU::xor_c,      // 0xa9
            &CPU::xor_d,      // 0xaa
            &CPU::xor_e,      // 0xab
            &CPU::xor_h,      // 0xac
            &CPU::xor_l,      // 0xad
            &CPU::xor_hlp,    // 0xae
            &CPU::xor_a,      // 0xaf
            &CPU::or_b,       // 0xb0
            &CPU::or_c,       // 0xb1
            &CPU::or_d,       // 0xb2
            &CPU::or_e,       // 0xb3
            &CPU::or_h,       // 0xb4
            &CPU::or_l,       // 0xb5
            &CPU::or_hlp,     // 0xb6
            &CPU::or_a,       // 0xb7
            &CPU::cp_b,       // 0xb8
            &CPU::cp_c,       // 0xb9
            &CPU::cp_d,       // 0xba
            &CPU::cp_e,       // 0xbb
            &CPU::cp_h,       // 0xbc
            &CPU::cp_l,       // 0xbd
            &CPU::cp_hlp,     // 0xbe
            &CPU::cp_a,       // 0xbf
            &CPU::ret_nz,     // 0xc0
            &CPU::pop_bc,     // 0xc1
            &CPU::jp_nz_nn,   // 0xc2
            &CPU::jp_nn,      // 0xc3
            &CPU::call_nz_nn, // 0xc4
            &CPU::push_bc,    // 0xc5
            &CPU::add_a_n,    // 0xc6
            &CPU::rst_0,      // 0xc7
            &CPU::ret_z,      // 0xc8
            &CPU::ret,        // 0xc9
            &CPU::jp_z_nn,    // 0xca
            &CPU::cb_n,       // 0xcb
            &CPU::call_z_nn,  // 0xcc
            &CPU::call_nn,    // 0xcd
            &CPU::adc_n,      // 0xce
            &CPU::rst_08,     // 0xcf
            &CPU::ret_nc,     // 0xd0
            &CPU::pop_de,     // 0xd1
            &CPU::jp_nc_nn,   // 0xd2
            &CPU::undefined,  // 0xd3
            &CPU::call_nc_nn, // 0xd4
            &CPU::push_de,    // 0xd5
            &CPU::sub_n,      // 0xd6
            &CPU::rst_10,     // 0xd7
            &CPU::ret_c,      // 0xd8
            &CPU::reti,       // 0xd9
            &CPU::jp_c_nn,    // 0xda
            &CPU::undefined,  // 0xdb
            &CPU::call_c_nn,  // 0xdc
            &CPU::undefined,  // 0xdd
            &CPU::sbc_n,      // 0xde
            &CPU::rst_18,     // 0xdf
            &CPU::ld_ff_n_a,  // 0xe0
            &CPU::pop_hl,     // 0xe1
            &CPU::ld_ff_c_a,  // 0xe2
            &CPU::undefined,  // 0xe3
            &CPU::undefined,  // 0xe4
            &CPU::push_hl,    // 0xe5
            &CPU::and_n,      // 0xe6
            &CPU::rst_20,     // 0xe7
            &CPU::add_sp_n,   // 0xe8
            &CPU::jp_hl,      // 0xe9
            &CPU::ld_nnp_a,   // 0xea
            &CPU::undefined,  // 0xeb
            &CPU::undefined,  // 0xec
            &CPU::undefined,  // 0xed
            &CPU::xor_n,      // 0xee
            &CPU::rst_28,     // 0xef
            &CPU::ld_a_ff_n,  // 0xf0
            &CPU::pop_af,     // 0xf1
            &CPU::ld_a_ff_c,  // 0xf2
            &CPU::di_inst,    // 0xf3
            &CPU::undefined,  // 0xf4
            &CPU::push_af,    // 0xf5
            &CPU::or_n,       // 0xf6
            &CPU::rst_30,     // 0xf7
            &CPU::ld_hl_sp_n, // 0xf8
            &CPU::ld_sp_hl,   // 0xf9
            &CPU::ld_a_nnp,   // 0xfa
            &CPU::ei,         // 0xfb
            &CPU::undefined,  // 0xfc
            &CPU::undefined,  // 0xfd
            &CPU::cp_n,       // 0xfe
            &CPU::rst_38,     // 0xff
    };

    void (CPU::*cbOpcodes[0x100])() = {
            &CPU::rlc_b,     // 0x00
            &CPU::rlc_c,     // 0x01
            &CPU::rlc_d,     // 0x02
            &CPU::rlc_e,     // 0x03
            &CPU::rlc_h,     // 0x04
            &CPU::rlc_l,     // 0x05
            &CPU::rlc_hlp,   // 0x06
            &CPU::rlc_a,     // 0x07
            &CPU::rrc_b,     // 0x08
            &CPU::rrc_c,     // 0x09
            &CPU::rrc_d,     // 0x0a
            &CPU::rrc_e,     // 0x0b
            &CPU::rrc_h,     // 0x0c
            &CPU::rrc_l,     // 0x0d
            &CPU::rrc_hlp,   // 0x0e
            &CPU::rrc_a,     // 0x0f
            &CPU::rl_b,      // 0x10
            &CPU::rl_c,      // 0x11
            &CPU::rl_d,      // 0x12
            &CPU::rl_e,      // 0x13
            &CPU::rl_h,      // 0x14
            &CPU::rl_l,      // 0x15
            &CPU::rl_hlp,    // 0x16
            &CPU::rl_a,      // 0x17
            &CPU::rr_b,      // 0x18
            &CPU::rr_c,      // 0x19
            &CPU::rr_d,      // 0x1a
            &CPU::rr_e,      // 0x1b
            &CPU::rr_h,      // 0x1c
            &CPU::rr_l,      // 0x1d
            &CPU::rr_hlp,    // 0x1e
            &CPU::rr_a,      // 0x1f
            &CPU::sla_b,     // 0x20
            &CPU::sla_c,     // 0x21
            &CPU::sla_d,     // 0x22
            &CPU::sla_e,     // 0x23
            &CPU::sla_h,     // 0x24
            &CPU::sla_l,     // 0x25
            &CPU::sla_hlp,   // 0x26
            &CPU::sla_a,     // 0x27
            &CPU::sra_b,     // 0x28
            &CPU::sra_c,     // 0x29
            &CPU::sra_d,     // 0x2a
            &CPU::sra_e,     // 0x2b
            &CPU::sra_h,     // 0x2c
            &CPU::sra_l,     // 0x2d
            &CPU::sra_hlp,   // 0x2e
            &CPU::sra_a,     // 0x2f
            &CPU::swap_b,    // 0x30
            &CPU::swap_c,    // 0x31
            &CPU::swap_d,    // 0x32
            &CPU::swap_e,    // 0x33
            &CPU::swap_h,    // 0x34
            &CPU::swap_l,    // 0x35
            &CPU::swap_hlp,  // 0x36
            &CPU::swap_a,    // 0x37
            &CPU::srl_b,     // 0x38
            &CPU::srl_c,     // 0x39
            &CPU::srl_d,     // 0x3a
            &CPU::srl_e,     // 0x3b
            &CPU::srl_h,     // 0x3c
            &CPU::srl_l,     // 0x3d
            &CPU::srl_hlp,   // 0x3e
            &CPU::srl_a,     // 0x3f
            &CPU::bit_0_b,   // 0x40
            &CPU::bit_0_c,   // 0x41
            &CPU::bit_0_d,   // 0x42
            &CPU::bit_0_e,   // 0x43
            &CPU::bit_0_h,   // 0x44
            &CPU::bit_0_l,   // 0x45
            &CPU::bit_0_hlp, // 0x46
            &CPU::bit_0_a,   // 0x47
            &CPU::bit_1_b,   // 0x48
            &CPU::bit_1_c,   // 0x49
            &CPU::bit_1_d,   // 0x4a
            &CPU::bit_1_e,   // 0x4b
            &CPU::bit_1_h,   // 0x4c
            &CPU::bit_1_l,   // 0x4d
            &CPU::bit_1_hlp, // 0x4e
            &CPU::bit_1_a,   // 0x4f
            &CPU::bit_2_b,   // 0x50
            &CPU::bit_2_c,   // 0x51
            &CPU::bit_2_d,   // 0x52
            &CPU::bit_2_e,   // 0x53
            &CPU::bit_2_h,   // 0x54
            &CPU::bit_2_l,   // 0x55
            &CPU::bit_2_hlp, // 0x56
            &CPU::bit_2_a,   // 0x57
            &CPU::bit_3_b,   // 0x58
            &CPU::bit_3_c,   // 0x59
            &CPU::bit_3_d,   // 0x5a
            &CPU::bit_3_e,   // 0x5b
            &CPU::bit_3_h,   // 0x5c
            &CPU::bit_3_l,   // 0x5d
            &CPU::bit_3_hlp, // 0x5e
            &CPU::bit_3_a,   // 0x5f
            &CPU::bit_4_b,   // 0x60
            &CPU::bit_4_c,   // 0x61
            &CPU::bit_4_d,   // 0x62
            &CPU::bit_4_e,   // 0x63
            &CPU::bit_4_h,   // 0x64
            &CPU::bit_4_l,   // 0x65
            &CPU::bit_4_hlp, // 0x66
            &CPU::bit_4_a,   // 0x67
            &CPU::bit_5_b,   // 0x68
            &CPU::bit_5_c,   // 0x69
            &CPU::bit_5_d,   // 0x6a
            &CPU::bit_5_e,   // 0x6b
            &CPU::bit_5_h,   // 0x6c
            &CPU::bit_5_l,   // 0x6d
            &CPU::bit_5_hlp, // 0x6e
            &CPU::bit_5_a,   // 0x6f
            &CPU::bit_6_b,   // 0x70
            &CPU::bit_6_c,   // 0x71
            &CPU::bit_6_d,   // 0x72
            &CPU::bit_6_e,   // 0x73
            &CPU::bit_6_h,   // 0x74
            &CPU::bit_6_l,   // 0x75
            &CPU::bit_6_hlp, // 0x76
            &CPU::bit_6_a,   // 0x77
            &CPU::bit_7_b,   // 0x78
            &CPU::bit_7_c,   // 0x79
            &CPU::bit_7_d,   // 0x7a
            &CPU::bit_7_e,   // 0x7b
            &CPU::bit_7_h,   // 0x7c
            &CPU::bit_7_l,   // 0x7d
            &CPU::bit_7_hlp, // 0x7e
            &CPU::bit_7_a,   // 0x7f
            &CPU::res_0_b,   // 0x80
            &CPU::res_0_c,   // 0x81
            &CPU::res_0_d,   // 0x82
            &CPU::res_0_e,   // 0x83
            &CPU::res_0_h,   // 0x84
            &CPU::res_0_l,   // 0x85
            &CPU::res_0_hlp, // 0x86
            &CPU::res_0_a,   // 0x87
            &CPU::res_1_b,   // 0x88
            &CPU::res_1_c,   // 0x89
            &CPU::res_1_d,   // 0x8a
            &CPU::res_1_e,   // 0x8b
            &CPU::res_1_h,   // 0x8c
            &CPU::res_1_l,   // 0x8d
            &CPU::res_1_hlp, // 0x8e
            &CPU::res_1_a,   // 0x8f
            &CPU::res_2_b,   // 0x90
            &CPU::res_2_c,   // 0x91
            &CPU::res_2_d,   // 0x92
            &CPU::res_2_e,   // 0x93
            &CPU::res_2_h,   // 0x94
            &CPU::res_2_l,   // 0x95
            &CPU::res_2_hlp, // 0x96
            &CPU::res_2_a,   // 0x97
            &CPU::res_3_b,   // 0x98
            &CPU::res_3_c,   // 0x99
            &CPU::res_3_d,   // 0x9a
            &CPU::res_3_e,   // 0x9b
            &CPU::res_3_h,   // 0x9c
            &CPU::res_3_l,   // 0x9d
            &CPU::res_3_hlp, // 0x9e
            &CPU::res_3_a,   // 0x9f
            &CPU::res_4_b,   // 0xa0
            &CPU::res_4_c,   // 0xa1
            &CPU::res_4_d,   // 0xa2
            &CPU::res_4_e,   // 0xa3
            &CPU::res_4_h,   // 0xa4
            &CPU::res_4_l,   // 0xa5
            &CPU::res_4_hlp, // 0xa6
            &CPU::res_4_a,   // 0xa7
            &CPU::res_5_b,   // 0xa8
            &CPU::res_5_c,   // 0xa9
            &CPU::res_5_d,   // 0xaa
            &CPU::res_5_e,   // 0xab
            &CPU::res_5_h,   // 0xac
            &CPU::res_5_l,   // 0xad
            &CPU::res_5_hlp, // 0xae
            &CPU::res_5_a,   // 0xaf
            &CPU::res_6_b,   // 0xb0
            &CPU::res_6_c,   // 0xb1
            &CPU::res_6_d,   // 0xb2
            &CPU::res_6_e,   // 0xb3
            &CPU::res_6_h,   // 0xb4
            &CPU::res_6_l,   // 0xb5
            &CPU::res_6_hlp, // 0xb6
            &CPU::res_6_a,   // 0xb7
            &CPU::res_7_b,   // 0xb8
            &CPU::res_7_c,   // 0xb9
            &CPU::res_7_d,   // 0xba
            &CPU::res_7_e,   // 0xbb
            &CPU::res_7_h,   // 0xbc
            &CPU::res_7_l,   // 0xbd
            &CPU::res_7_hlp, // 0xbe
            &CPU::res_7_a,   // 0xbf
            &CPU::set_0_b,   // 0xc0
            &CPU::set_0_c,   // 0xc1
            &CPU::set_0_d,   // 0xc2
            &CPU::set_0_e,   // 0xc3
            &CPU::set_0_h,   // 0xc4
            &CPU::set_0_l,   // 0xc5
            &CPU::set_0_hlp, // 0xc6
            &CPU::set_0_a,   // 0xc7
            &CPU::set_1_b,   // 0xc8
            &CPU::set_1_c,   // 0xc9
            &CPU::set_1_d,   // 0xca
            &CPU::set_1_e,   // 0xcb
            &CPU::set_1_h,   // 0xcc
            &CPU::set_1_l,   // 0xcd
            &CPU::set_1_hlp, // 0xce
            &CPU::set_1_a,   // 0xcf
            &CPU::set_2_b,   // 0xd0
            &CPU::set_2_c,   // 0xd1
            &CPU::set_2_d,   // 0xd2
            &CPU::set_2_e,   // 0xd3
            &CPU::set_2_h,   // 0xd4
            &CPU::set_2_l,   // 0xd5
            &CPU::set_2_hlp, // 0xd6
            &CPU::set_2_a,   // 0xd7
            &CPU::set_3_b,   // 0xd8
            &CPU::set_3_c,   // 0xd9
            &CPU::set_3_d,   // 0xda
            &CPU::set_3_e,   // 0xdb
            &CPU::set_3_h,   // 0xdc
            &CPU::set_3_l,   // 0xdd
            &CPU::set_3_hlp, // 0xde
            &CPU::set_3_a,   // 0xdf
            &CPU::set_4_b,   // 0xe0
            &CPU::set_4_c,   // 0xe1
            &CPU::set_4_d,   // 0xe2
            &CPU::set_4_e,   // 0xe3
            &CPU::set_4_h,   // 0xe4
            &CPU::set_4_l,   // 0xe5
            &CPU::set_4_hlp, // 0xe6
            &CPU::set_4_a,   // 0xe7
            &CPU::set_5_b,   // 0xe8
            &CPU::set_5_c,   // 0xe9
            &CPU::set_5_d,   // 0xea
            &CPU::set_5_e,   // 0xeb
            &CPU::set_5_h,   // 0xec
            &CPU::set_5_l,   // 0xed
            &CPU::set_5_hlp, // 0xee
            &CPU::set_5_a,   // 0xef
            &CPU::set_6_b,   // 0xf0
            &CPU::set_6_c,   // 0xf1
            &CPU::set_6_d,   // 0xf2
            &CPU::set_6_e,   // 0xf3
            &CPU::set_6_h,   // 0xf4
            &CPU::set_6_l,   // 0xf5
            &CPU::set_6_hlp, // 0xf6
            &CPU::set_6_a,   // 0xf7
            &CPU::set_7_b,   // 0xf8
            &CPU::set_7_c,   // 0xf9
            &CPU::set_7_d,   // 0xfa
            &CPU::set_7_e,   // 0xfb
            &CPU::set_7_h,   // 0xfc
            &CPU::set_7_l,   // 0xfd
            &CPU::set_7_hlp, // 0xfe
            &CPU::set_7_a,   // 0xff
    };
    
    Gameboy* gameboy;

    int retVal;

    u64 cycleCount;
    u64 eventCycle;

    struct Registers registers;

    bool haltState;
    bool haltBug;

    bool ime;
};