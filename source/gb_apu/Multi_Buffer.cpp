// Blip_Buffer 0.4.1. http://www.slack.net/~ant/

#include "gb_apu/Multi_Buffer.h"
#include "types.h"

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

// Tracked_Blip_Buffer

Tracked_Blip_Buffer::Tracked_Blip_Buffer()
{
    last_non_silence = 0;
}

void Tracked_Blip_Buffer::clear()
{
    last_non_silence = 0;
    Blip_Buffer::clear();
}

void Tracked_Blip_Buffer::end_frame( s32 t )
{
    Blip_Buffer::end_frame( t );
    if ( clear_modified() )
        last_non_silence = samples_avail() + blip_buffer_extra_;
}

u32 Tracked_Blip_Buffer::non_silent() const
{
    return last_non_silence | unsettled();
}

inline void Tracked_Blip_Buffer::remove_( long n )
{
    if ( (last_non_silence -= n) < 0 )
        last_non_silence = 0;
}

void Tracked_Blip_Buffer::remove_silence( long n )
{
    remove_( n );
    Blip_Buffer::remove_silence( n );
}

void Tracked_Blip_Buffer::remove_samples( long n )
{
    remove_( n );
    Blip_Buffer::remove_samples( n );
}

long Tracked_Blip_Buffer::read_samples( s16* out, long count )
{
    count = Blip_Buffer::read_samples( out, count );
    remove_( count );
    return count;
}

// Stereo_Buffer

Stereo_Buffer::Stereo_Buffer()
{
    mixer.bufs [2] = &bufs [2];
    mixer.bufs [0] = &bufs [0];
    mixer.bufs [1] = &bufs [1];
    mixer.samples_read = 0;
}

const char* Stereo_Buffer::set_sample_rate( long rate, int msec )
{
    mixer.samples_read = 0;
    for ( int i = bufs_size; --i >= 0; ) {
        const char* err = bufs[i].set_sample_rate(rate, msec);
        if(err) {
            return err;
        }
    }

    return 0;
}

void Stereo_Buffer::clock_rate( long rate )
{
    for ( int i = bufs_size; --i >= 0; )
        bufs [i].clock_rate( rate );
}

void Stereo_Buffer::bass_freq( int bass )
{
    for ( int i = bufs_size; --i >= 0; )
        bufs [i].bass_freq( bass );
}

void Stereo_Buffer::clear()
{
    mixer.samples_read = 0;
    for ( int i = bufs_size; --i >= 0; )
        bufs [i].clear();
}

void Stereo_Buffer::end_frame( s32 time )
{
    for ( int i = bufs_size; --i >= 0; )
        bufs [i].end_frame( time );
}

long Stereo_Buffer::read_samples( s16* out, long out_size )
{
    assert( (out_size & 1) == 0 ); // must read an even number of samples
    out_size = out_size < samples_avail() ? out_size : samples_avail();

    int pair_count = int (out_size >> 1);
    if ( pair_count )
    {
        mixer.read_pairs( out, pair_count );

        for ( int i = bufs_size; --i >= 0; )
        {
            Tracked_Blip_Buffer& b = bufs [i];
            // TODO: might miss non-silence settling since it checks END of last read
            if ( !b.non_silent() )
                b.remove_silence( mixer.samples_read );
            else
                b.remove_samples( mixer.samples_read );
        }
        mixer.samples_read = 0;
    }
    return out_size;
}

// Stereo_Mixer

// mixers use a single index value to improve performance on register-challenged processors
// offset goes from negative to zero

void Stereo_Mixer::read_pairs( s16* out, int count )
{
    // TODO: if caller never marks buffers as modified, uses mono
    // except that buffer isn't cleared, so caller can encounter
    // subtle problems and not realize the cause.
    samples_read += count;
    if ( bufs [0]->non_silent() | bufs [1]->non_silent() )
        mix_stereo( out, count );
    else
        mix_mono( out, count );
}

void Stereo_Mixer::mix_mono( s16* out_, int count )
{
    int const bass = BLIP_READER_BASS( *bufs [2] );
    BLIP_READER_BEGIN( center, *bufs [2] );
    BLIP_READER_ADJ_( center, samples_read );

    s16* BLIP_RESTRICT out = out_ + count * 2;
    int offset = -count;
    do
    {
        s32 s = BLIP_READER_READ( center );
        BLIP_READER_NEXT_IDX_( center, bass, offset );
        BLIP_CLAMP( s, s );

        out [offset * 2 + 0] = (s16) s;
        out [offset * 2 + 1] = (s16) s;
    }
    while ( ++offset );

    BLIP_READER_END( center, *bufs [2] );
}

void Stereo_Mixer::mix_stereo( s16* out_, int count )
{
    s16* BLIP_RESTRICT out = out_ + count * 2;

    // do left + center and right + center separately to reduce register load
    Tracked_Blip_Buffer* const* buf = &bufs [2];
    while ( true ) // loop runs twice
    {
        --buf;
        --out;

        int const bass = BLIP_READER_BASS( *bufs [2] );
        BLIP_READER_BEGIN( side,   **buf );
        BLIP_READER_BEGIN( center, *bufs [2] );

        BLIP_READER_ADJ_( side,   samples_read );
        BLIP_READER_ADJ_( center, samples_read );

        int offset = -count;
        do
        {
            s32 s = BLIP_READER_READ_RAW( center ) + BLIP_READER_READ_RAW( side );
            s >>= blip_sample_bits - 16;
            BLIP_READER_NEXT_IDX_( side,   bass, offset );
            BLIP_READER_NEXT_IDX_( center, bass, offset );
            BLIP_CLAMP( s, s );

            ++offset; // before write since out is decremented to slightly before end
            out [offset * 2] = (s16) s;
        }
        while ( offset );

        BLIP_READER_END( side,   **buf );

        if ( buf != bufs )
            continue;

        // only end center once
        BLIP_READER_END( center, *bufs [2] );
        break;
    }
}