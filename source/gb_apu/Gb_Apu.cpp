// Gb_Snd_Emu 0.2.0. http://www.slack.net/~ant/

#include "gb_apu/Gb_Apu.h"

/* Copyright (C) 2003-2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include <assert.h>
#include <string.h>

unsigned const vol_reg    = 0xFF24;
unsigned const stereo_reg = 0xFF25;
unsigned const status_reg = 0xFF26;
unsigned const wave_ram   = 0xFF30;

int const power_mask = 0x80;

void Gb_Apu::treble_eq( blip_eq_t const& eq )
{
	good_synth.treble_eq( eq );
	med_synth .treble_eq( eq );
}

inline int Gb_Apu::calc_output( int osc ) const
{
	int bits = regs [stereo_reg - start_addr] >> osc;
	return (bits >> 3 & 2) | (bits & 1);
}

void Gb_Apu::set_output( Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right, int osc )
{
	// Must be silent (all NULL), mono (left and right NULL), or stereo (none NULL)
	assert( !center || (center && !left && !right) || (center && left && right) );
	assert( (unsigned) osc <= osc_count ); // fails if you pass invalid osc index
	
	if ( !center || !left || !right )
	{
		left  = center;
		right = center;
	}
	
	int i = (unsigned) osc % osc_count;
	do
	{
		Gb_Osc& o = *oscs [i];
		o.outputs [1] = right;
		o.outputs [2] = left;
		o.outputs [3] = center;
		o.output = o.outputs [calc_output( i )];
	}
	while ( ++i < osc );
}

void Gb_Apu::set_osc_enabled(int osc, bool enabled) {
    oscs_enabled[osc] = enabled;
}

void Gb_Apu::synth_volume( int iv )
{
	double v = volume_ * 0.60 / osc_count / 15 /*steps*/ / 8 /*master vol range*/ * iv;
	good_synth.volume( v );
	med_synth .volume( v );
}

void Gb_Apu::apply_volume()
{
	// TODO: Doesn't handle differing left and right volumes (panning).
	// Not worth the complexity.
	int data  = regs [vol_reg - start_addr];
	int left  = data >> 4 & 7;
	int right = data & 7;
	//if ( data & 0x88 ) dprintf( "Vin: %02X\n", data & 0x88 );
	//if ( left != right ) dprintf( "l: %d r: %d\n", left, right );
	synth_volume( (left > right ? left : right) + 1 );
}

void Gb_Apu::volume( double v )
{
	if ( volume_ != v )
	{
		volume_ = v;
		apply_volume();
	}
}

void Gb_Apu::reset_regs()
{
	for ( int i = 0; i < 0x20; i++ )
		regs [i] = 0;

	square1.reset();
	square2.reset();
	wave   .reset();
	noise  .reset();
	
	apply_volume();
}

void Gb_Apu::reset_lengths()
{
	square1.length_ctr = 64;
	square2.length_ctr = 64;
	wave   .length_ctr = 256;
	noise  .length_ctr = 64;
}

void Gb_Apu::reduce_clicks( bool reduce )
{
	reduce_clicks_ = reduce;
	
	// Click reduction makes DAC off generate same output as volume 0
	int dac_off_amp = 0;
	if ( reduce && wave.mode != mode_agb ) // AGB already eliminates clicks
		dac_off_amp = -Gb_Osc::dac_bias;
	
	for ( int i = 0; i < osc_count; i++ )
		oscs [i]->dac_off_amp = dac_off_amp;
	
	// AGB always eliminates clicks on wave channel using same method
	if ( wave.mode == mode_agb )
		wave.dac_off_amp = -Gb_Osc::dac_bias;
}

void Gb_Apu::reset( mode_t mode, bool agb_wave )
{
	// Hardware mode
	if ( agb_wave )
		mode = mode_agb; // using AGB wave features implies AGB hardware
	wave.agb_mask = agb_wave ? 0xFF : 0;
	for ( int i = 0; i < osc_count; i++ )
		oscs [i]->mode = mode;
	reduce_clicks( reduce_clicks_ );
	
	// Reset state
	frame_time  = 0;
	last_time   = 0;
	frame_phase = 0;
	
	reset_regs();
	reset_lengths();
	
	// Load initial wave RAM
	static unsigned char const initial_wave [2] [16] = {
		{0x84,0x40,0x43,0xAA,0x2D,0x78,0x92,0x3C,0x60,0x59,0x59,0xB0,0x34,0xB8,0x2E,0xDA},
		{0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF},
	};
	for ( int b = 2; --b >= 0; )
	{
		// Init both banks (does nothing if not in AGB mode)
		// TODO: verify that this works
		write_register( 0, 0xFF1A, b * 0x40 );
		for ( unsigned i = 0; i < sizeof initial_wave [0]; i++ )
			write_register( 0, i + wave_ram, initial_wave [(mode != mode_dmg)] [i] );
	}
}

void Gb_Apu::set_tempo( double t )
{
	frame_period = 4194304 / 512; // 512 Hz
	if ( t != 1.0 )
		frame_period = s32 (frame_period / t);
}

Gb_Apu::Gb_Apu()
{
	wave.wave_ram = &regs [wave_ram - start_addr];
	
	oscs [0] = &square1;
	oscs [1] = &square2;
	oscs [2] = &wave;
	oscs [3] = &noise;
	
	for ( int i = osc_count; --i >= 0; )
	{
		Gb_Osc& o = *oscs [i];
		o.regs        = &regs [i * 5];
		o.output      = 0;
		o.outputs [0] = 0;
		o.outputs [1] = 0;
		o.outputs [2] = 0;
		o.outputs [3] = 0;
		o.good_synth  = &good_synth;
		o.med_synth   = &med_synth;
	}
	
	reduce_clicks_ = false;
	set_tempo( 1.0 );
	volume_ = 1.0;
	reset();
}

void Gb_Apu::run_until_( s32 end_time )
{
	while ( true )
	{
		// run oscillators
		s32 time = end_time;
		if ( time > frame_time )
			time = frame_time;

        if(oscs_enabled[0]) {
            square1.run(last_time, time);
        }

        if(oscs_enabled[1]) {
            square2.run(last_time, time);
        }

        if(oscs_enabled[2]) {
            wave.run(last_time, time);
        }

        if(oscs_enabled[3]) {
            noise.run(last_time, time);
        }

		last_time = time;
		
		if ( time == end_time )
			break;
		
		// run frame sequencer
		frame_time += frame_period;
		switch ( frame_phase++ )
		{
		case 2:
		case 6:
			// 128 Hz
			square1.clock_sweep();
		case 0:
		case 4:
			// 256 Hz
			square1.clock_length();
			square2.clock_length();
			wave   .clock_length();
			noise  .clock_length();
			break;
		
		case 7:
			// 64 Hz
			frame_phase = 0;
			square1.clock_envelope();
			square2.clock_envelope();
			noise  .clock_envelope();
		}
	}
}

inline void Gb_Apu::run_until( s32 time )
{
	assert( time >= last_time ); // end_time must not be before previous time
	if ( time > last_time )
		run_until_( time );
}

void Gb_Apu::end_frame( s32 end_time )
{
	if ( end_time > last_time )
		run_until( end_time );
	
	frame_time -= end_time;
	assert( frame_time >= 0 );
	
	last_time -= end_time;
	assert( last_time >= 0 );
}

void Gb_Apu::silence_osc( Gb_Osc& o )
{
	int delta = -o.last_amp;
	if ( delta )
	{
		o.last_amp = 0;
		if ( o.output )
		{
			o.output->set_modified();
			med_synth.offset( last_time, delta, o.output );
		}
	}
}

void Gb_Apu::apply_stereo()
{
	for ( int i = osc_count; --i >= 0; )
	{
		Gb_Osc& o = *oscs [i];
		Blip_Buffer* out = o.outputs [calc_output( i )];
		if ( o.output != out )
		{
			silence_osc( o );
			o.output = out;
		}
	}
}

void Gb_Apu::write_register( s32 time, unsigned addr, int data )
{
	assert( (unsigned) data < 0x100 );
	
	int reg = addr - start_addr;
	if ( (unsigned) reg >= register_count )
	{
		assert( false );
		return;
	}
	
	if ( addr < status_reg && !(regs [status_reg - start_addr] & power_mask) )
	{
		// Power is off
		
		// length counters can only be written in DMG mode
		if ( wave.mode != mode_dmg || (reg != 1 && reg != 5+1 && reg != 10+1 && reg != 15+1) )
			return;
		
		if ( reg < 10 )
			data &= 0x3F; // clear square duty
	}
	
	run_until( time );
	
	if ( addr >= wave_ram )
	{
		wave.write( addr, data );
	}
	else
	{
		int old_data = regs [reg];
		regs [reg] = data;
		
		if ( addr < vol_reg )
		{
			// Oscillator
			write_osc( reg / 5, reg, old_data, data );
		}
		else if ( addr == vol_reg && data != old_data )
		{
			// Master volume
			for ( int i = osc_count; --i >= 0; )
				silence_osc( *oscs [i] );
			
			apply_volume();
		}
		else if ( addr == stereo_reg )
		{
			// Stereo panning
			apply_stereo();
		}
		else if ( addr == status_reg && (data ^ old_data) & power_mask )
		{
			// Power control
			frame_phase = 0;
			for ( int i = osc_count; --i >= 0; )
				silence_osc( *oscs [i] );
		
			reset_regs();
			if ( wave.mode != mode_dmg )
				reset_lengths();
			
			regs [status_reg - start_addr] = data;
		}
	}
}

int Gb_Apu::read_register( s32 time, unsigned addr )
{
	run_until( time );
	
	int reg = addr - start_addr;
	assert( (unsigned) reg < register_count );
	
	if ( addr >= wave_ram )
		return wave.read( addr );
	
	// Value read back has some bits always set
	static unsigned char const masks [] = {
		0x80,0x3F,0x00,0xFF,0xBF,
		0xFF,0x3F,0x00,0xFF,0xBF,
		0x7F,0xFF,0x9F,0xFF,0xBF,
		0xFF,0xFF,0x00,0x00,0xBF,
		0x00,0x00,0x70,
		0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
	};
	int mask = masks [reg];
	if ( wave.agb_mask && (reg == 10 || reg == 12) )
		mask = 0x1F; // extra implemented bits in wave regs on AGB
	int data = regs [reg] | mask;
	
	// Status register
	if ( addr == status_reg )
	{
		data &= 0xF0;
		data |= (int) square1.enabled << 0;
		data |= (int) square2.enabled << 1;
		data |= (int) wave   .enabled << 2;
		data |= (int) noise  .enabled << 3;
	}
	
	return data;
}

#define REFLECT( x, y ) ({                                                     \
    if(save) {                                                                 \
		(y)[0] = (u8) ((x) >> 0);                                              \
	    (y)[1] = (u8) ((x) >> 8);                                              \
	    (y)[2] = (u8) ((x) >> 16);                                             \
	    (y)[3] = (u8) ((x) >> 24);                                             \
    } else {                                                                   \
        (x) = ((y)[0] << 0) | ((y)[1] << 8) | ((y)[2] << 16) | ((y)[3] << 24); \
    }                                                                          \
})

inline const char* Gb_Apu::save_load( gb_apu_state_t* io, bool save )
{
	assert( sizeof (gb_apu_state_t) == 256 );

	int format = io->format0;
	REFLECT( format, io->format );
	if ( format != io->format0 )
		return "Unsupported sound save state format";

	int version = 0;
	REFLECT( version, io->version );

	// Registers and wave RAM
	assert( regs_size == sizeof io->regs );
	if ( save )
		memcpy( io->regs, regs, sizeof io->regs );
	else
		memcpy( regs, io->regs, sizeof     regs );

	// Frame sequencer
	REFLECT( frame_time,  io->frame_time  );
	REFLECT( frame_phase, io->frame_phase );

	REFLECT( square1.sweep_freq,    io->sweep_freq );
	REFLECT( square1.sweep_delay,   io->sweep_delay );
	REFLECT( square1.sweep_enabled, io->sweep_enabled );
	REFLECT( square1.sweep_neg,     io->sweep_neg );

	REFLECT( noise.divider,         io->noise_divider );
	REFLECT( wave.sample_buf,       io->wave_buf );

	for ( int i = osc_count; --i >= 0; )
	{
		Gb_Osc& osc = *oscs [i];
		REFLECT( osc.delay,      io->delay      [i] );
		REFLECT( osc.length_ctr, io->length_ctr [i] );
		REFLECT( osc.phase,      io->phase      [i] );
		REFLECT( osc.enabled,    io->enabled    [i] );

		if ( i != 2 )
		{
			int j = i < 2 ? i : 2;
			Gb_Env& env = static_cast<Gb_Env&>(osc);
			REFLECT( env.env_delay,   io->env_delay   [j] );
			REFLECT( env.volume,      io->env_volume  [j] );
			REFLECT( env.env_enabled, io->env_enabled [j] );
		}
	}

	return 0;
}

void Gb_Apu::save_state( gb_apu_state_t* out )
{
	(void) save_load( out, true );
	memset( out->unused, 0, sizeof out->unused );
}

const char* Gb_Apu::load_state( gb_apu_state_t const& in )
{
	const char* err = save_load( const_cast<gb_apu_state_t*>(&in), false );
	if(err) {
		return err;
	}

	apply_stereo();
	synth_volume( 0 );          // suppress output for the moment
	run_until_( last_time );    // get last_amp updated
	apply_volume();             // now use correct volume

	return 0;
}